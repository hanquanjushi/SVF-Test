#include <unistd.h>

int main()
{
    char buffer[256];

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