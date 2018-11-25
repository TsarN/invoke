#ifndef INVOKE_INVOKERPROFILE_HPP
#define INVOKE_INVOKERPROFILE_HPP

#include <string>
#include <vector>

enum class PathAccess {
    ReadWrite,
    ReadOnly,
    ReadOnlySecurity,
    Denied,
    Security,
};

enum class SyscallAction {
    Unspecified,
    Allow,
    CheckPath,
    CheckPathSecurity,
    Deny,
    Security,
};

struct PathPermission {
    std::string path;
    PathAccess access;
};

struct SyscallPolicy {
    std::string syscall;
    SyscallAction action;
};

struct InvokerProfile {
    std::string name;
    SyscallAction defaultSyscallAction;
    PathAccess defaultPathAccess;
    std::vector<SyscallPolicy> syscalls;
    std::vector<PathPermission> paths;

    static const std::vector<std::string> availableProfiles;

    static const InvokerProfile& getProfile(std::string name);
};

#endif //INVOKE_INVOKERPROFILE_HPP
