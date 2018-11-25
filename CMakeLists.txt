cmake_minimum_required(VERSION 3.2)
project(invoke)

set(CMAKE_CXX_STANDARD 11)

include_directories(src)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED program_options filesystem)

add_executable(invoke src/main.cpp src/invokers/ptrace/PtraceInvoker.cpp
        src/invokers/ptrace/PtraceInvoker.hpp src/InvokerConfig.cpp
        src/InvokerConfig.hpp src/InvokerProfile.hpp src/profiles/linux_native.hpp src/SyscallTable.hpp
        src/tables/linux_i386.hpp src/tables/linux_x86_64.hpp
        src/invokers/ptrace/PtraceInvoker_x86_64.cpp src/invokers/ptrace/PtraceInvoker_x86_64.hpp
        src/tables/linux_x86_64.cpp src/tables/linux_i386.cpp
        src/profiles/linux_native.cpp
        src/InvokerProfile.cpp src/Invoker.cpp src/Invoker.hpp)
target_link_libraries(invoke ${Boost_LIBRARIES})