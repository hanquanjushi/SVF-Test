import os
import subprocess

# 编译所有的C文件
c_files = ["LinkedList", "main", "parser", "utils"]
o_files = []

for file in c_files:
    subprocess.run(["gclang", "-g", "-c", f"{file}.c", "-o", f"{file}.o"])
    o_files.append(f"{file}.o")

# 将所有的.o文件链接在一起
subprocess.run(["gclang", "-g"] + o_files + ["-o", "my_program"])

# 使用get-bc命令生成.bc文件
subprocess.run(["get-bc", "-b", "my_program"])