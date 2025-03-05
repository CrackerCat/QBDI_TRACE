//
// Created by fang on 23-12-19.
//



#include <iostream>
#include <iomanip>
#include <cassert>
#include <sstream>
#include <unordered_map>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <string>
#include <QBDI/Callback.h>
#include "vm.h"
#include "logger.h"
#include "dobby_symbol_resolver.h"
#include "HookInfo.h"
#include "assert.h"
#include "hexdump.h"
#include "utils.h"

using namespace std;
using namespace QBDI;


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




// 显示指令执行前的寄存器状态
QBDI::VMAction showPreInstruction(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto thiz = (class vm *) data;

    // 获取当前指令的分析信息
    const QBDI::InstAnalysis *instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_INSTRUCTION | QBDI::ANALYSIS_SYMBOL | QBDI::ANALYSIS_DISASSEMBLY | QBDI::ANALYSIS_OPERANDS);

    // 输出符号名和偏移量，如果没有符号，则仅输出地址和反汇编信息
    if (instAnalysis->symbol != nullptr) {
        thiz->logbuf << instAnalysis->symbol << "[0x" << std::hex << instAnalysis->symbolOffset << "]:0x" << instAnalysis->address << ": " << instAnalysis->disassembly;
    } else {
        std::string symbolInfo = getSymbolFromCache(instAnalysis->address);
        if (!symbolInfo.empty()) {
            thiz->logbuf << symbolInfo << ":0x" << std::hex << instAnalysis->address << ": " << instAnalysis->disassembly;
        } else {
            // 如果 /proc/self/maps 中也找不到对应信息，仅输出地址和反汇编信息
            thiz->logbuf << "0x" << std::hex << instAnalysis->address << ": " << instAnalysis->disassembly;
        }
    }

    std::stringstream output;
    // 遍历操作数并记录读取的寄存器状态
    for (int i = 0; i < instAnalysis->numOperands; ++i) {
        auto op = instAnalysis->operands[i];
        if (op.regAccess == QBDI::REGISTER_READ || op.regAccess == REGISTER_READ_WRITE) {
            if (op.regCtxIdx != -1 && op.type == OPERAND_GPR) {
                // 将寄存器名称和值添加到输出流
                output << op.regName << "=0x" << std::hex << QBDI_GPR_GET(gprState, op.regCtxIdx) << " ";
                output.flush();
            }
        }
    }

    // 如果有读取的寄存器信息，格式化输出
    if (!output.str().empty()) {
        thiz->logbuf << "\tr[" << output.str() << "]";
    }
    return QBDI::VMAction::CONTINUE;
}

// 显示指令执行后的寄存器状态 打印字符串 hexdump
QBDI::VMAction showPostInstruction(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto thiz = (class vm *) data;

    // 获取当前指令的分析信息，包括指令、符号、操作数等
    const QBDI::InstAnalysis *instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_INSTRUCTION | QBDI::ANALYSIS_SYMBOL | QBDI::ANALYSIS_DISASSEMBLY | QBDI::ANALYSIS_OPERANDS);

    std::stringstream output;
    std::stringstream regOutput;

    // 遍历操作数并记录写入的寄存器状态
    for (int i = 0; i < instAnalysis->numOperands; ++i) {
        auto op = instAnalysis->operands[i];
        if (op.regAccess == REGISTER_WRITE || op.regAccess == REGISTER_READ_WRITE) {
            if (op.regCtxIdx != -1 && op.type == OPERAND_GPR) {
                // 获取寄存器值
                uint64_t regValue = QBDI_GPR_GET(gprState, op.regCtxIdx);

                // 输出寄存器名称和值
                output << op.regName << "=0x" << std::hex << regValue << " ";
                output.flush();

                // 对可能为地址的寄存器值进行 hexdump 或字符串输出，仅在值为有效地址时执行
                if (isValidAddress(regValue)) {
                    const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(regValue);
                    size_t maxLen = 256;  // 最大显示字节数
                    uint8_t buffer[256];
                    if (safeReadMemory(regValue, buffer, maxLen)) {
                        if (isAsciiPrintableString(buffer, maxLen)) {
                            regOutput << "Strings :" << std::string(reinterpret_cast<const char*>(buffer)) << "\n";
                        } else {
                            regOutput << "Hexdump for " << op.regName << " at address 0x" << std::hex << regValue << ":\n";
                            hexdump_memory(regOutput, buffer, 32, regValue);  // 显示32字节内容
                        }
                    } else {
                        regOutput << "Invalid memory access at address 0x" << std::hex << regValue << "\n";
                    }
                }
            }
        }
    }

    // 如果有写入的寄存器信息，格式化输出；否则，仅换行
    if (!output.str().empty()) {
        thiz->logbuf << "\tw[" << output.str() << "]" << std::endl;
        thiz->logbuf << regOutput.str();
    } else {
        thiz->logbuf << std::endl;
        thiz->logbuf << regOutput.str();
    }
    return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction
showMemoryAccess(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState,
                 void *data) {
    auto thiz = (class vm *) data;
    if (vm->getInstMemoryAccess().empty()) {
        thiz->logbuf << std::endl;
    }
    for (const auto &acc: vm->getInstMemoryAccess()) {
        if (acc.type == MEMORY_READ) {
            thiz->logbuf << "   mem[r]:0x" << std::hex << acc.accessAddress << " size:" << acc.size
                         << " value:0x" << acc.value;
        } else if (acc.type == MEMORY_WRITE) {
            thiz->logbuf << "   mem[w]:0x" << std::hex << acc.accessAddress << " size:" << acc.size
                         << " value:0x" << acc.value;
        } else {
            thiz->logbuf << "   mem[rw]:0x" << std::hex << acc.accessAddress << " size:" << acc.size
                         << " value:0x" << acc.value;
        }
    }
    thiz->logbuf << std::endl << std::endl;
    return QBDI::VMAction::CONTINUE;
}



QBDI::VM vm::init(void* address) {
    uint32_t cid;
    QBDI::GPRState *state;
    QBDI::VM qvm{};
    // Get a pointer to the GPR state of the VM
    state = qvm.getGPRState();
    loadMemoryRanges();
    assert(state != nullptr);
    qvm.recordMemoryAccess(QBDI::MEMORY_READ_WRITE);

    cid = qvm.addCodeCB(QBDI::PREINST, showPreInstruction, this);
    assert(cid != QBDI::INVALID_EVENTID);


    cid = qvm.addCodeCB(QBDI::POSTINST, showPostInstruction, this);
    assert(cid != QBDI::INVALID_EVENTID);

    // 添加内存访问回调
    cid = qvm.addMemAccessCB(MEMORY_READ_WRITE, showMemoryAccess, this);
    assert(cid != QBDI::INVALID_EVENTID);

    bool ret = qvm.addInstrumentedModuleFromAddr(reinterpret_cast<QBDI::rword>(address));
    assert(ret == true);

    return qvm;
}


void syn_regs(DobbyRegisterContext *ctx,QBDI::GPRState *state,bool D2Q){
    if (D2Q){
        for(int i = 0 ; i < 29; i++){
            QBDI_GPR_SET(state,i,ctx->general.x[i]);
        }
        state->lr = ctx->lr;
        state->x29 = ctx->fp;
        state->sp = ctx->sp;

//        state->sp = ctx->sp;

    }else{
        for(int i = 0 ; i < 29; i++){
            //QBDI_GPR_SET(state,i,ctx->general.x[i]);
            ctx->general.x[i] = QBDI_GPR_GET(state,i);
        }
        ctx->lr = state->lr;
        ctx->fp = state->x29;
        ctx->sp = state->sp;

    }
    //state->sp = ctx->sp;
}









