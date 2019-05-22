#!/usr/bin/env python3
# Generate profiles for invoke

import json

profiles = ["linux_native", "linux_nosecurity"]


def gen(name):
    with open(name + ".json") as f:
        data = json.load(f)

    syscalls = []
    syscalls_default = "Security"

    paths = []
    paths_default = "Security"

    for t in data.get("syscalls", []):
        if t == "default":
            syscalls_default = data["syscalls"][t]
            continue
        action = "SyscallAction::{}".format(t)
        for syscall in data["syscalls"][t]:
            syscalls.append((syscall, action))

    for t in data.get("paths", []):
        if t == "default":
            paths_default = data["paths"][t]
            continue
        access = "PathAccess::{}".format(t)
        for path in data["paths"][t]:
            paths.append((path, access))

    with open(name + ".cpp", "w") as f:
        print("// This file is generated automatically. Any changes will be lost!", file=f)
        print("#include \"profiles/%s.hpp\"" % name, file=f)
        print(file=f)

        print("const InvokerProfile %s_profile = {" % name, file=f)
        print("    \"%s\", SyscallAction::%s, PathAccess::%s," % (name, syscalls_default, paths_default), file=f)
        print("    {", file=f)
        for syscall, action in syscalls:
            print("        { \"%s\", %s }," % (syscall, action), file=f)
        print("    },", file=f)
        print("    {", file=f)
        for path, access in paths:
            print("        { \"%s\", %s }," % (path, access), file=f)
        print("    },", file=f)
        print("};", file=f)

    with open(name + ".hpp", "w") as f:
        guard = "_PROFILE_{}_INCLUDED_".format(name.upper())
        print("// This file is generated automatically. Any changes will be lost!", file=f)
        print(file=f)
        print("#ifndef {}".format(guard), file=f)
        print("#define {}".format(guard), file=f)
        print(file=f)
        print("#include \"InvokerProfile.hpp\"", file=f)
        print(file=f)
        print("extern const InvokerProfile %s_profile;" % name, file=f)
        print(file=f)
        print("#endif // {}".format(guard), file=f)

def main():
    for p in profiles:
        gen(p)


if __name__ == "__main__":
    main()
