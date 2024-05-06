import os
import subprocess

# 编译所有的C文件
c_files = ["LinkedList.c", "main.c", "parser.c", "utils.c"]
for file in c_files:
    subprocess.run(["gclang", "-g", f"{file}.o", file])

# 提取LLVM bitcode
bc_files = []
for file in c_files:
    subprocess.run(["get-bc", "-o", f"{file}.bc", f"{file}.o"])
    bc_files.append(f"{file}.bc")

# 链接所有的LLVM bitcode文件
subprocess.run(["llvm-link", "-o", "output.ll"] + bc_files)