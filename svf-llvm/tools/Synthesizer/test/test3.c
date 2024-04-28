#include <unistd.h>
#include <fcntl.h>

int main() {
    char buffer[256];
    int file = open("example.txt", O_RDONLY);  // 打开一个名为 "example.txt" 的文件以读取数据

    if (file < 0) {  // 如果文件打开失败，open() 会返回 -1
        return 1;
    }

    ssize_t bytesRead = read(file, buffer, sizeof(buffer) - 1);  // 从文件中读取数据

    if (bytesRead >= 0) {
        buffer[bytesRead] = '\0';  // 在字符串尾部添加 null 字符
    } else {
        // 处理错误
        return 1;
    }

    close(file);  // 关闭文件
    return 0;
}