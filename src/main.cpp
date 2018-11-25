#include <iostream>
#include <invokers/linux_native/PtraceInvoker_x86_64.hpp>
#include <profiles/linux_native.hpp>
#include <fcntl.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char **argv) {
    double timeLimit = 0;
    long memoryLimit = 0;
    double wallLimit = 0;
    std::string stdinFilename;
    std::string stdoutFilename;
    std::string stderrFilename;
    std::vector<std::string> program, env;

    std::string profileName = InvokerProfile::availableProfiles[0];
    std::string invokerName = Invoker::availableInvokers[0].first;
    std::string archName = Invoker::availableInvokers[0].second[0];

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help,h", "Show help message")
            ("list-profiles", "Show list of available profiles")
            ("list-invokers", "Show list of available invokers")
            ("profile,P", po::value<std::string>(&profileName), "Set profile used to run program")
            ("invoker,I", po::value<std::string>(&invokerName), "Set invoker used to run program")
            ("arch,a", po::value<std::string>(&archName))
            ("verbose,v", "Be more verbose")
            ("stdin,i", po::value<std::string>(&stdinFilename), "Redirect program's stdin to this file instead of stdin")
            ("stdout,o", po::value<std::string>(&stdoutFilename), "Redirect program's stdout to this file instead of stdout")
            ("stderr,e", po::value<std::string>(&stderrFilename), "Redirect program's stderr to this file instead of discarding it")
            ("time-limit,t", po::value<double>(&timeLimit), "CPU time limit in seconds")
            ("memory-limit,m", po::value<long>(&memoryLimit), "Memory limit in megabytes")
            ("wall-limit,w", po::value<double>(&wallLimit), "Wall time limit in seconds")
            ("environment,E", po::value<std::vector<std::string>>(&env), "Set environment variable")
            ("inherit-environment,r", "Inherit current environment variables");

    po::options_description hidden;
    hidden.add_options()
            ("program", po::value<std::vector<std::string>>(&program));

    po::options_description all_options;
    all_options.add(desc);
    all_options.add(hidden);

    po::positional_options_description pdesc;
    pdesc.add("program", -1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
                .options(all_options)
                .positional(pdesc).run(), vm);
        po::notify(vm);
    } catch (po::error &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    memoryLimit *= 1024 * 1024;

    if (vm.count("list-invokers")) {
        for (auto &invoker : Invoker::availableInvokers) {
            std::cout << invoker.first << " (available architectures: ";
            bool first = true;
            for (auto &arch : invoker.second) {
                if (!first) {
                    std::cout << ", ";
                }
                std::cout << arch;
                first = false;
            }
            std::cout << ")" << std::endl;
        }
        return 0;
    }

    if (vm.count("list-profiles")) {
        for (auto &profile : InvokerProfile::availableProfiles) {
            std::cout << profile << std::endl;
        }
        return 0;
    }

    if (vm.count("help") || program.empty()) {
        std::cerr << "Usage: " << argv[0] << " [options] <program> [arguments]" << std::endl;
        std::cerr << std::endl;
        std::cerr << desc << std::endl;
        return 1;
    }

    Invoker *invoker;
    try {
        InvokerConfig config(InvokerProfile::getProfile(profileName));
        config.wallLimit = wallLimit;
        config.timeLimit = timeLimit;
        config.memoryLimit = memoryLimit;
        config.exe = program[0];
        config.args = program;
        config.envp = env;
        config.inheritEnvironment = vm.count("inherit-environment") > 0;
        config.log = vm.count("verbose") > 0;
        config.stderr_fd = -1;

        if (!stdinFilename.empty()) {
            config.stdin_fd = open(stdinFilename.c_str(), O_RDONLY);
            if (config.stdin_fd < 0) {
                std::cerr << "Failed to open file '" << stdinFilename << "' for reading: " << strerror(errno) << std::endl;
                return 1;
            }
        }

        if (!stdoutFilename.empty()) {
            config.stdout_fd = open(stdoutFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (config.stdout_fd < 0) {
                std::cerr << "Failed to open file '" << stdoutFilename << "' for writing: " << strerror(errno) << std::endl;
                return 1;
            }
        }

        if (!stderrFilename.empty()) {
            config.stderr_fd = open(stderrFilename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (config.stderr_fd < 0) {
                std::cerr << "Failed to open file '" << stderrFilename << "' for writing: " << strerror(errno) << std::endl;
                return 1;
            }
        }

        invoker = Invoker::makeInvoker(invokerName, archName, config);
        invoker->run();
        if (!config.log) {
            invoker->printResults();
        }

        if (config.stdin_fd > 2) {
            close(config.stdin_fd);
        }

        if (config.stdout_fd > 2) {
            close(config.stdout_fd);
        }

        if (config.stderr_fd > 2) {
            close(config.stderr_fd);
        }
    } catch (std::invalid_argument &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}