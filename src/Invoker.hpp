#ifndef INVOKE_INVOKER_HPP
#define INVOKE_INVOKER_HPP

#include "InvokerConfig.hpp"

class Invoker {
public:
    virtual void run() = 0;
    virtual void printResults() const = 0;
    virtual const InvokerResult& getResult() const = 0;

    static const std::vector<std::pair<std::string, std::vector<std::string>>> availableInvokers;
    static Invoker *makeInvoker(const std::string &name, const std::string &arch, const InvokerConfig &config);
};


#endif //INVOKE_INVOKER_HPP
