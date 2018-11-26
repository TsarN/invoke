#ifndef INVOKE_PTRACEINVOKER_X86_64_HPP
#define INVOKE_PTRACEINVOKER_X86_64_HPP


#include "PtraceInvoker.hpp"

class PtraceInvoker_x86_64 : public PtraceInvoker {
protected:
    int regSyscall() const override;
    int regSyscallResult() const override;
    int regArg0() const override;
    int regArg1() const override;
    int regArg2() const override;
    int syscallFromName(const std::string &name) const override;

public:
    explicit PtraceInvoker_x86_64(const InvokerConfig &config);
};


#endif //INVOKE_PTRACEINVOKER_X86_64_HPP
