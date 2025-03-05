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

// 地址范围的结构体，用于缓存 maps 文件中的每个模块范围
struct MemoryRange {
    uint64_t startAddr;
    uint64_t endAddr;
    std::string pathname;
};

// 缓存地址范围和已解析的符号信息
std::unordered_map<uint64_t, std::string> symbolCache;
std::vector<MemoryRange> memoryRanges;

// 从 /proc/self/maps 读取并缓存模块地址范围
void loadMemoryRanges() {
    std::ifstream mapsFile("/proc/self/maps");
    std::string line;

    while (std::getline(mapsFile, line)) {
        std::istringstream iss(line);
        std::string addrRange, perms, offset, dev, inode, pathname;
        uint64_t startAddr, endAddr;

        // 解析 maps 文件的一行内容
        iss >> addrRange >> perms >> offset >> dev >> inode;
        if (!(iss >> pathname)) {
            pathname = ""; // 如果没有路径，将其设为空字符串
        }


        size_t pos = pathname.find_last_of('/');
        std::string filename = (pos == std::string::npos) ? pathname : pathname.substr(pos + 1);


        // 解析地址范围
        std::replace(addrRange.begin(), addrRange.end(), '-', ' ');
        std::istringstream addrStream(addrRange);
        addrStream >> std::hex >> startAddr >> endAddr;

        // 添加到缓存的内存范围
        memoryRanges.push_back({startAddr, endAddr, filename});
    }
}

// 从缓存中查找地址范围内的符号信息
std::string getSymbolFromCache(uint64_t address) {
    // 检查缓存
    if (symbolCache.find(address) != symbolCache.end()) {
        return symbolCache[address];
    }

    // 遍历已缓存的地址范围
    for (const auto& range : memoryRanges) {
        if (address >= range.startAddr && address < range.endAddr) {
            uint64_t addrOffset = address - range.startAddr;
            std::ostringstream symbolStream;
            symbolStream << range.pathname << "[0x" << std::hex << addrOffset << "]";
            std::string symbol = symbolStream.str();

            // 将结果存入缓存
            symbolCache[address] = symbol;
            return symbol;
        }
    }

    // 未找到时返回空字符串
    symbolCache[address] = "";  // 记录无符号信息，避免重复查找
    return "";
}

// 判断地址是否在有效内存页上
bool isValidAddress(uint64_t address) {
    if (address == 0x7603511e){
//        LOGE("address : %p",address);
//        return false;
    }
    // 获取系统页大小
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        return false;
    }

    // 对齐地址到页大小
    void* alignedAddress = reinterpret_cast<void*>(address & ~(pageSize - 1));
    unsigned char vec;

    // 使用 mincore 检查该地址是否为有效内存页
    if (mincore(alignedAddress, pageSize, &vec) == 0) {
        return true;  // 地址有效
    }
    return false;  // 地址无效
}


// 判断内存内容是否为有效的 ASCII 可打印字符串，且不为全空格
bool isAsciiPrintableString(const uint8_t* data, size_t length) {
    if (data == nullptr) {
        LOGD("Error: data is NULL");
        return false;  // 数据为空，返回无效字符串
    }

    bool hasNonSpaceChar = false;  // 标记是否包含非空格字符
    for (size_t i = 0; i < length; ++i) {
        if (data[i] == '\0') {
            return hasNonSpaceChar;  // 如果遇到终止符，且包含非空格字符，则认为是有效字符串
        }
        if (data[i] < 0x20 || data[i] > 0x7E) {
            return false;  // 如果包含非 ASCII 可打印字符，视为无效字符串
        }
        if (data[i] != ' ') {
            hasNonSpaceChar = true;  // 检测到非空格字符
        }
    }
    return hasNonSpaceChar;  // 字符串没有终止符时，检查是否包含非空格字符
}

// 使用 process_vm_readv 安全读取内存的函数
bool safeReadMemory(uint64_t address, uint8_t* buffer, size_t length) {
    struct iovec local_iov;
    struct iovec remote_iov;

    local_iov.iov_base = buffer;
    local_iov.iov_len = length;

    remote_iov.iov_base = reinterpret_cast<void*>(address);
    remote_iov.iov_len = length;

    ssize_t nread = process_vm_readv(getpid(), &local_iov, 1, &remote_iov, 1, 0);
    if (nread == static_cast<ssize_t>(length)) {
        return true;  // 成功读取
    } else {
        // 可以记录错误信息，便于调试
//        LOGE("process_vm_readv failed at address 0x%lx: %s", address, strerror(errno));
        return false;  // 读取失败
    }
}




#endif //XPOSEDNHOOK_HEXDUMP_H
