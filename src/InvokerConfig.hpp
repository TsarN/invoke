#ifndef INVOKE_INVOKERCONFIG_HPP
#define INVOKE_INVOKERCONFIG_HPP

#include <string>
#include <vector>
#include "InvokerProfile.hpp"

class InvokerConfig {
public:
    explicit InvokerConfig(const InvokerProfile &profile);

    double timeLimit = -1;
    double wallLimit = -1;
    long memoryLimit = -1;

    int stdin_fd = 0;
    int stdout_fd = 1;
    int stderr_fd = 2;

    bool log = false;

    std::string exe;
    std::string workingDirectory;
    std::vector<std::string> args;
    std::vector<std::string> envp;

    std::vector<std::string> writeableFiles;

    bool inheritEnvironment = false;
    const InvokerProfile &profile;
};

class InvokerResult {
public:
    int exitCode = 0;
    int error = 0;
    std::string errorMessage;

    double cpuUsage = 0.0;
    double wallClock = 0.0;
    long memoryUsage = 0;

    bool securityViolation = false;
    bool timeLimitExceeded = false;
    bool wallLimitExceeded = false;
    bool memoryLimitExceeded = false;
};


#endif //INVOKE_INVOKERCONFIG_HPP
