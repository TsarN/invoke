#include <stdexcept>
#include <invokers/linux_native/PtraceInvoker_x86_64.hpp>
#include <sstream>
#include "Invoker.hpp"

const std::vector<std::pair<std::string, std::vector<std::string>>> Invoker::availableInvokers = {
        { "ptrace", { "x86_64" } },
};

Invoker *Invoker::makeInvoker(
        const std::string &name,
        const std::string &arch,
        const InvokerConfig &config) {
    if (name == "ptrace") {
        if (arch == "x86_64") {
            return new PtraceInvoker_x86_64(config);
        }
    }

    std::stringstream buf;
    buf << "Invoker '" << name << "' with architecture '" << arch << "' does not exist.";

    throw std::invalid_argument(buf.str());
}
