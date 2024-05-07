#include <stdio.h>
#include <unistd.h>
int main()
{
    char buffer[256];
    // 打开一个二进制文件
    FILE* file = __builtin__fopen("example.bin", "rb");
    if (file == NULL)
    {
    }

    // 从文件中读取数据
    size_t result = fread(buffer, 1, sizeof(buffer), file);
    if (result != sizeof(buffer))
    {
        // 读取数据失败或到达文件末尾
    }

    // 关闭文件
    __builtin__fclose(file);
    __builtin__read(0, buffer, sizeof(buffer) - 1);
    ssize_t bytesRead = __builtin__read(0, buffer, sizeof(buffer) - 1); // 从文件中读取数据

    if (bytesRead >= 0)
    {
        buffer[bytesRead] = '\0'; // 在字符串尾部添加 null 字符
    }
    else
    {
        // 处理错误
        return 1;
    }

    return 0;
}