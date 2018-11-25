#!/usr/bin/env python3
# Generate syscall tables

tables = ["linux_i386", "linux_x86_64"]


def gen(name):
    table = []
    with open(name + ".tbl") as f:
        for line in f:
            num, syscall = line.split()
            table.append((int(num), syscall))

    with open(name + ".cpp", "w") as f:
        print("// This file is generated automatically. Any changes will be lost!", file=f)
        print("#include \"tables/%s.hpp\"" % name, file=f)
        print(file=f)

        print("const SyscallTable %s_table = {" % (name), file=f)
        for num, syscall in table:
            print("    { \"%s\", %d }," % (syscall, num), file=f)
        print("};", file=f)

        print(file=f)

    with open(name + ".hpp", "w") as f:
        guard = "_TABLE_{}_INCLUDED_".format(name.upper())
        print("// This file is generated automatically. Any changes will be lost!", file=f)
        print(file=f)
        print("#ifndef {}".format(guard), file=f)
        print("#define {}".format(guard), file=f)
        print(file=f)
        print("#include \"SyscallTable.hpp\"", file=f)
        print(file=f)

        print("extern const SyscallTable %s_table;" % (name), file=f)

        print(file=f)
        print("#endif // {}".format(guard), file=f)


def main():
    for t in tables:
        gen(t)

if __name__ == "__main__":
    main()
