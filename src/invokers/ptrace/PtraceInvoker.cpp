#include "PtraceInvoker.hpp"

#include <iostream>
#include <cstring>
#include <vector>
#include <cmath>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/time.h>

PtraceInvoker::PtraceInvoker(const InvokerConfig &config) : config(config) {

}

void PtraceInvoker::run() {
    buildSyscallTable();

    pid_t timeout_pid = -1;
    if (config.wallLimit > 0) {
        timeout_pid = fork();
        if (timeout_pid < 0) {
            result.error = errno;
            result.errorMessage = std::string("Failed to fork: ") + strerror(errno);
            if (config.log) {
                std::cerr << result.errorMessage << std::endl;
            }
            return;
        }

        if (timeout_pid == 0) {
            timeout();
        }
    }

    int fdUp[2], fdDown[2];
    pipe(fdUp);
    pipe(fdDown);

    pid_t child_pid = fork();

    if (child_pid < 0) {
        result.error = errno;
        result.errorMessage = std::string("Failed to fork: ") + strerror(errno);
        if (config.log) {
            std::cerr << result.errorMessage << std::endl;
        }
        if (timeout_pid != -1) {
            kill(timeout_pid, SIGKILL);
            waitpid(timeout_pid, NULL, 0);
        }
        close(fdUp[0]);
        close(fdUp[1]);
        close(fdDown[0]);
        close(fdDown[1]);
        return;
    }

    if (child_pid == 0) {
        tracee(fdDown, fdUp);
    }

    tracer(child_pid, timeout_pid, fdDown, fdUp);
}

void PtraceInvoker::timeout() {
    usleep((useconds_t)(config.wallLimit * 1e6));
    exit(0);
}

void PtraceInvoker::tracee(int downPipe[2], int upPipe[2]) {
    close(downPipe[1]);
    close(upPipe[0]);

    int rpipe = downPipe[0];
    int wpipe = upPipe[1];

    int lock;
    read(rpipe, &lock, sizeof(lock));
    close(rpipe);
    fcntl(wpipe, F_SETFD, FD_CLOEXEC);

    if (!doChdir() ||
        !dupFile(STDIN_FILENO, config.stdin_fd) ||
        !dupFile(STDOUT_FILENO, config.stdout_fd) ||
        !dupFile(STDERR_FILENO, config.stderr_fd)) {

        write(wpipe, &result.error, sizeof(result.error));
        exit(1);
    }

    if (config.timeLimit > 0) {
        auto cpu = (rlim_t)ceil(config.timeLimit);
        // use infinite hard limit, so we can catch SIGXCPUs
        setLimit(RLIMIT_CPU, cpu, RLIM_INFINITY);
    }

    if (config.memoryLimit > 0) {
        auto mem = (rlim_t)config.memoryLimit;
        mem *= 2; // we need it to detect memory limits
        setLimit(RLIMIT_AS, mem, mem);
    }

    std::vector<char*> cargs;
    cargs.reserve(config.args.size());
    for (const std::string &s : config.args) {
        char *t = new char[s.size() + 1];
        strcpy(t, s.c_str());
        cargs.push_back(t);
    }
    cargs.push_back(NULL);

    if (config.inheritEnvironment) {
        execv(config.exe.c_str(), &cargs[0]);
    } else {
        std::vector<char*> cenvp;
        cenvp.reserve(config.envp.size());
        for (const std::string &s : config.envp) {
            char *t = new char[s.size() + 1];
            strcpy(t, s.c_str());
            cenvp.push_back(t);
        }
        cenvp.push_back(NULL);
        execve(config.exe.c_str(), &cargs[0], &cenvp[0]);
    }

    int err = errno;
    write(wpipe, &err, sizeof(err));
    close(wpipe);
    exit(1);
}

void PtraceInvoker::tracer(pid_t pid, pid_t timer, int downPipe[2], int upPipe[2]) {
    close(downPipe[0]);
    close(upPipe[1]);

    int rpipe = upPipe[0];
    int wpipe = downPipe[1];

    int err = 0;
    int status = 0;
    didExec = false;
    inSyscall = false;
    denySyscall = false;

    if (config.workingDirectory.empty()) {
        char *cwd_ = get_current_dir_name();
        cwd = cwd_;
        free(cwd_);
    } else {
        boost::system::error_code err;
        cwd = boost::filesystem::canonical(config.workingDirectory, err);
        if (err.value()) {
            result.error = err.value();
            result.errorMessage = "Failed to resolve working directory " + err.message();
            if (config.log) {
                std::cerr << result.errorMessage << std::endl;
            }
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            if (timer != -1) {
                kill(timer, SIGKILL);
                waitpid(timer, NULL, 0);
            }
        }
    }

    // attach to tracee
    ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    waitpid(pid, &status, 0);
    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
        // we have successfully attached to the tracee

        ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_EXITKILL);
        ptrace(PTRACE_CONT, pid, NULL, NULL); // we will stop at the next execve()
    } else {
        // something has gone seriously wrong
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        if (timer != -1) {
            kill(timer, SIGKILL);
            waitpid(timer, NULL, 0);
        }
        result.error = -1;
        result.errorMessage =
                "Failed to attach to tracee: waitpid() returned invalid status " +
                std::to_string(status);
        if (config.log) {
            std::cerr << result.errorMessage << std::endl;
        }
        return;
    }

    write(wpipe, &err, sizeof(err));
    close(wpipe);

    if (read(rpipe, &err, sizeof(err)) > 0) {
        // execve() failed
        close(rpipe);
        if (timer != -1) {
            kill(timer, SIGKILL);
            waitpid(timer, NULL, 0);
        }
        result.error = err;
        result.errorMessage = std::string("execve() failed: ") + strerror(err);
        if (config.log) {
            std::cerr << result.errorMessage << std::endl;
        }
        return;
    }
    close(rpipe);

    result.cpuUsage = 0.0;
    result.wallClock = 0.0;
    result.memoryUsage = 0;
    double start = getTime();

    for (;;) {
        rusage ru;
        pid_t p = wait3(&status, 0, &ru);

        if (timer != -1 && p == timer) {
            if (WIFEXITED(status)) {
                if (config.log) {
                    std::cerr << "Timer thread died. Wall Time Limit exceeded" << std::endl;
                }
                result.wallLimitExceeded = true;
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                break;
            }
        }

        if (p == pid) {
            double cpuUsage = tvToSeconds(ru.ru_utime);
            if (result.cpuUsage < cpuUsage)
                result.cpuUsage = cpuUsage;

            long memoryUsage = ru.ru_maxrss * 1024L;
            if (result.memoryUsage < memoryUsage)
                result.memoryUsage = memoryUsage;

            bool quit = false;

            if (config.timeLimit > 0 && result.cpuUsage >= config.timeLimit) {
                quit = true;
            }

            if (config.memoryLimit > 0 && result.memoryUsage >= config.memoryLimit) {
                quit = true;
            }

            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
                if (config.log) {
                    std::cerr << "Tracee died with exit code " << result.exitCode << std::endl;
                }
                if (timer != -1) {
                    kill(timer, SIGKILL);
                    waitpid(timer, NULL, 0);
                }
                break;
            }

            if (WIFSIGNALED(status)) {
                result.exitCode = -WTERMSIG(status);
                if (config.log) {
                    std::cerr << "Tracee died with signal " << -result.exitCode << std::endl;
                }
                if (timer != -1) {
                    kill(timer, SIGKILL);
                    waitpid(timer, NULL, 0);
                }
                break;
            }

            if (WIFSTOPPED(status)) {
                int sendSignal = WSTOPSIG(status);
                if (WSTOPSIG(status) == SIGTRAP) {
                    // ptrace event
                    sendSignal = 0;
                    if (!onTrap(pid)) {
                        // security violation, kill it
                        if (config.log) {
                            std::cerr << "Tracee made dangerous system call. Security Violation" << std::endl;
                        }
                        result.securityViolation = true;
                        quit = true;
                    }
                }

                if (WSTOPSIG(status) == SIGXCPU) {
                    // cpu limit exceeded
                    if (config.log) {
                        std::cerr << "Tracee received SIGXCPU. Time Limit Exceeded" << std::endl;
                    }
                    result.timeLimitExceeded = true;
                    quit = true;
                }

                if (!quit) {
                    ptrace(PTRACE_SYSCALL, pid, NULL, sendSignal);
                }
            }

            if (quit) {
                kill(pid, SIGKILL);
                waitpid(pid, NULL, 0);
                if (timer != -1) {
                    kill(timer, SIGKILL);
                    waitpid(timer, NULL, 0);
                }
                break;
            }
        }
    }

    result.wallClock = getTime() - start;

    if (config.timeLimit > 0 && result.cpuUsage >= config.timeLimit) {
        result.timeLimitExceeded = true;
    }

    if (config.memoryLimit > 0 && result.memoryUsage >= config.memoryLimit) {
        result.memoryLimitExceeded = true;
    }

    if (config.wallLimit > 0 && result.wallClock >= config.wallLimit) {
        result.wallLimitExceeded = true;
    }

    if (config.log) {
        printResults();
    }
}

bool PtraceInvoker::dupFile(int prev, int next) {
    if (next < 0) {
        if (close(prev) < 0) {
            result.error = errno;
            return false;
        }
    } else if (prev != next) {
        if (dup2(next, prev) < 0) {
            result.error = errno;
            return false;
        }
        if (close(next) < 0) {
            result.error = errno;
            return false;
        }
    }
    return true;
}

void PtraceInvoker::setLimit(__rlimit_resource limit, rlim_t soft, rlim_t hard) {
    rlimit lim = {
            soft,
            hard,
    };
    setrlimit(limit, &lim);
}

std::string PtraceInvoker::traceeReadString(pid_t pid, void *addr, size_t max_size) {
    char *ptr = (char*)addr;
    std::string res;
    if (!ptr) {
        return res;
    }
    while (res.size() < max_size) {
        long word = ptrace(PTRACE_PEEKDATA, pid, ptr, NULL);
        if (word == -1) {
            break;
        }
        ptr += sizeof(long);
        char *c = (char*)&word;
        for (int n = 0; n < sizeof(word); ++c, ++n) {
            if (*c >= 32 && *c <= 127) {
                res.push_back(*c);
            } else {
                return res;
            }
        }
    }
    return res;
}

const InvokerResult &PtraceInvoker::getResult() const {
    return result;
}

double PtraceInvoker::getTime() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tvToSeconds(tv);
}

double PtraceInvoker::tvToSeconds(timeval tv) {
    return 1.0 * tv.tv_sec + 1e-6 * tv.tv_usec;
}

bool PtraceInvoker::onTrap(pid_t pid) {
    if (!didExec) {
        // tracee did execve(), transition into secure state
        didExec = true;
        return true;
    }
    inSyscall = !inSyscall;

    if (!inSyscall) {
        if (denySyscall != 0) {
            // return an error as syscall result
            ptrace(PTRACE_POKEUSER, pid, regSyscallResult(), (void*)denySyscall);
            denySyscall = 0;
        }
        return true;
    }

    long syscall = ptrace(PTRACE_PEEKUSER, pid, regSyscall(), NULL);

    if (syscall < 0 || syscall >= SYSCALL_MAX) {
        return true;
    }

    SyscallAction action = syscallActions[syscall];
    if (action == SyscallAction::Unspecified) {
        action = config.profile.defaultSyscallAction;
    }

    if (action == SyscallAction::Allow) {
        // just allow
        return true;
    }

    bool security = false;

    if (action == SyscallAction::Security || action == SyscallAction::CheckPathSecurity) {
        security = true;
    }

    if (action == SyscallAction::CheckPath || action == SyscallAction::CheckPathSecurity) {
        void *ptr = (void*)ptrace(PTRACE_PEEKUSER, pid, (syscall == syscall_openat) ? regArg1() : regArg0(), NULL);
        std::string str_path = traceeReadString(pid, ptr);
        boost::filesystem::path path = str_path;

        if (path.is_relative())
            path = cwd / path;
        path = path.lexically_normal();

        if (config.log) {
            std::cerr << path << std::endl;
        }

        auto access = checkPath(path);

        if (access == PathAccess::Security) {
            security = true;
        }

        if (access == PathAccess::ReadWrite) {
            return true;
        }

        if (access == PathAccess::ReadOnly || access == PathAccess::ReadOnlySecurity) {
            if (syscall == syscall_open || syscall == syscall_openat) {
                // check if we don't request write permissions
                long arg = (syscall == syscall_openat) ? regArg2() : regArg1();
                long flags = ptrace(PTRACE_PEEKUSER, pid, arg, NULL);

                if (!(flags & O_WRONLY) && !(flags & O_RDWR)) {
                    return true;
                }

                // permission error
                if (access == PathAccess::ReadOnlySecurity) {
                    security = true;
                }
            } else {
                return true;
            }
        }

        denySyscall = -EACCES;
    }

    // emulate syscall, return error
    if (config.log) {
        std::cerr << "denied syscall " << syscall << std::endl;
    }
    if (denySyscall == 0) {
        denySyscall = -EPERM;
    }
    ptrace(PTRACE_POKEUSER, pid, regSyscall(), (void*)-1); // drop this syscall
    return !security;
}

PathAccess PtraceInvoker::checkPath(boost::filesystem::path path) {
    PathAccess access = config.profile.defaultPathAccess;

    if (path.parent_path() == cwd) {
        auto filename = path.filename();
        for (const std::string &file : config.writeableFiles) {
            if (filename == file) {
                return PathAccess::ReadWrite;
            }
        }
        return PathAccess::ReadOnly;
    }

    for (const PathPermission &permission : config.profile.paths) {
        if (permission.path.empty()) continue;
        boost::filesystem::path cur(permission.path);
        if (permission.path.back() == '/') {
            // directory
            if (cur.filename() == ".") {
                cur.remove_filename();
            }
            auto cur_len = std::distance(cur.begin(), cur.end());
            auto path_len = std::distance(path.begin(), path.end());
            if (cur_len > path_len) {
                continue;
            }
            if (std::equal(cur.begin(), cur.end(), path.begin())) {
                access = permission.access;
            }
        } else {
            // file
            if (cur == path) {
                access = permission.access;
            }
        }
    }

    return access;
}

void PtraceInvoker::buildSyscallTable() {
    syscall_open = syscallFromName("open");
    syscall_openat = syscallFromName("openat");

    for (const SyscallPolicy &policy : config.profile.syscalls) {
        long syscall = syscallFromName(policy.syscall);
        if (syscall >= 0 && syscall < SYSCALL_MAX) {
            syscallActions[syscall] = policy.action;
        }
    }
}

void PtraceInvoker::printResults() const {
    if (result.error != 0) {
        std::cerr << "--- Error ---" << std::endl;
        std::cerr << result.errorMessage << std::endl;
    }
    std::cerr << "--- Result ---" << std::endl;
    std::cerr << "Exit code: " << result.exitCode << std::endl;
    std::cerr << "Wall clock: " << result.wallClock << " sec" << std::endl;
    std::cerr << "Cpu usage: " << result.cpuUsage << " sec" << std::endl;
    std::cerr << "Memory usage: " << result.memoryUsage << " bytes" << std::endl;

    std::cerr << "--- Verdicts ---" << std::endl;
    if (result.timeLimitExceeded) {
        std::cerr << "Time Limit Exceeded" << std::endl;
    }

    if (result.memoryLimitExceeded) {
        std::cerr << "Memory Limit Exceeded" << std::endl;
    }

    if (result.wallLimitExceeded) {
        std::cerr << "Wall Clock Limit Exceeded" << std::endl;
    }

    if (result.securityViolation) {
        std::cerr << "Security Violation" << std::endl;
    }
}

bool PtraceInvoker::doChdir() {
    if (config.workingDirectory.empty())
        return true;
    return chdir(config.workingDirectory.c_str()) == -1;
}
