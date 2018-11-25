#ifndef INVOKE_PTRACEINVOKER_HPP
#define INVOKE_PTRACEINVOKER_HPP


#include <InvokerConfig.hpp>
#include <sys/resource.h>
#include <array>
#include <boost/filesystem.hpp>
#include <Invoker.hpp>

class PtraceInvoker : public Invoker {
private:
    static const int SYSCALL_MAX = 4096;
    std::array<SyscallAction, SYSCALL_MAX> syscallActions;

    bool inSyscall;
    bool didExec;
    long denySyscall;

    long syscall_open;
    long syscall_openat;

    boost::filesystem::path cwd;

    const InvokerConfig &config;
    InvokerResult result;

    void tracee(int downPipe[2], int upPipe[2]);
    void tracer(pid_t pid, pid_t timer, int downPipe[2], int upPipe[2]);
    PathAccess checkPath(boost::filesystem::path path);
    bool onTrap(pid_t pid);
    void timeout();
    double getTime();
    double tvToSeconds(timeval tv);
    bool dupFile(int prev, int next);
    bool doChdir();
    void setLimit(__rlimit_resource limit, rlim_t soft, rlim_t hard);
    std::string traceeReadString(pid_t pid, void *addr, size_t max_size = 4096);
    void buildSyscallTable();

protected:
    virtual int regSyscall() = 0;
    virtual int regSyscallResult() = 0;
    virtual int regArg0() = 0;
    virtual int regArg1() = 0;
    virtual int regArg2() = 0;
    virtual int syscallFromName(const std::string &name) = 0;

public:
    explicit PtraceInvoker(const InvokerConfig &config);
    void run() override;
    void printResults() override;
    const InvokerResult& getResult() override;
};


#endif //INVOKE_PTRACEINVOKER_HPP