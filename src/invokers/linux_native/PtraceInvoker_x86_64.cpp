#include "PtraceInvoker_x86_64.hpp"
#include "tables/linux_x86_64.hpp"

#include <sys/reg.h>

int PtraceInvoker_x86_64::regSyscall() {
    return sizeof(long) * ORIG_RAX;
}

int PtraceInvoker_x86_64::regSyscallResult() {
    return sizeof(long) * RAX;
}

int PtraceInvoker_x86_64::regArg0() {
    return sizeof(long) * RDI;
}

int PtraceInvoker_x86_64::regArg1() {
    return sizeof(long) * RSI;
}

int PtraceInvoker_x86_64::regArg2() {
    return sizeof(long) * RDX;
}

int PtraceInvoker_x86_64::syscallFromName(const std::string &name) {
    auto it = linux_x86_64_table.find(name);
    if (it == linux_x86_64_table.end()) {
        return -1;
    }
    return it->second;
}

PtraceInvoker_x86_64::PtraceInvoker_x86_64(const InvokerConfig &config) : PtraceInvoker(config) {
}
