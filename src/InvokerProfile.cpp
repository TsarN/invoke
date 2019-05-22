#include <profiles/linux_native.hpp>
#include <profiles/linux_nosecurity.hpp>
#include <stdexcept>
#include <sstream>
#include "InvokerProfile.hpp"

const std::vector<std::string> InvokerProfile::availableProfiles = {
    "linux_native",
    "linux_nosecurity"
};

const InvokerProfile &InvokerProfile::getProfile(std::string name) {
    if (name == "linux_native") {
        return linux_native_profile;
    }
    if (name == "linux_nosecurity") {
        return linux_nosecurity_profile;
    }
    std::stringstream buf;
    buf << "Invalid profile name '" << name << "'";
    throw std::invalid_argument(buf.str());
}
