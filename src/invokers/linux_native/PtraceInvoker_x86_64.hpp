#ifndef INVOKE_PTRACEINVOKER_X86_64_HPP
#define INVOKE_PTRACEINVOKER_X86_64_HPP


#include "PtraceInvoker.hpp"

class PtraceInvoker_x86_64 : public PtraceInvoker {
protected:
    int regSyscall() override;
    int regSyscallResult() override;
    int regArg0() override;
    int regArg1() override;
    int regArg2() override;
    int syscallFromName(const std::string &name) override;

public:
    explicit PtraceInvoker_x86_64(const InvokerConfig &config);
};


#endif //INVOKE_PTRACEINVOKER_X86_64_HPP
