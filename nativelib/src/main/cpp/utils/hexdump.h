//
// Created by 李狗蛋 on 11/7/24.
//

#ifndef XPOSEDNHOOK_HEXDUMP_H
#define XPOSEDNHOOK_HEXDUMP_H


#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>

#include <sys/mman.h>  // for mincore
#include <unistd.h>    // for sysconf


// 将内存块按 hexdump 格式输出到日志缓冲区
void hexdump_memory(std::stringstream &logbuf, const uint8_t* data, size_t size, uint64_t address) {
    size_t offset = 0;

    while (offset < size) {
        // 输出当前行的基址
        logbuf << std::hex << std::setw(8) << std::setfill('0') << (address + offset) << ": ";

        // 输出每一行的十六进制数据和ASCII字符
        std::string ascii; // 暂存 ASCII 字符串
        for (size_t i = 0; i < 16; ++i) {
            if (offset + i < size) {
                uint8_t byte = data[offset + i];
                logbuf << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(byte) << " ";
                ascii += (std::isprint(byte) ? static_cast<char>(byte) : '.'); // 可打印字符直接显示，否则用 .
            } else {
                logbuf << "   "; // 如果数据不足16字节，填充空格
                ascii += " ";
            }
            if (i == 7) logbuf << " "; // 中间分隔
        }

        // 输出 ASCII 表示
        logbuf << " |" << ascii << "|" << std::endl;
        offset += 16;
    }
}




#endif //XPOSEDNHOOK_HEXDUMP_H
