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
#include "traceUtils.h"
#include "utils.h"
#include "syscall_table.h"

using namespace std;
using namespace QBDI;


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
                uint64_t regValue = QBDI_GPR_GET(gprState, op.regCtxIdx);

                // 将寄存器名称和值添加到输出流
                output << op.regName << "=0x" << std::hex << regValue << " ";
                // 检测是否是 LR（x30） 并获取符号
                if (strcmp(op.regName, "LR") == 0) {
                    std::string symbolInfo = getSymbolFromCache(regValue);
                    if (!symbolInfo.empty()) {
                        output << " -> Symbol: " << symbolInfo << " ";
                    }
                }
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
                // 检测是否是 LR（x30） 并获取符号
                if (strcmp(op.regName, "LR") == 0) {
                    std::string symbolInfo = getSymbolFromCache(regValue);
                    if (!symbolInfo.empty()) {
                        output << " -> Symbol: " << symbolInfo << " ";
                    }
                }
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

QBDI::VMAction showMemoryAccess(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    auto thiz = (class vm *) data;
    if (vm->getInstMemoryAccess().empty()) {
        thiz->logbuf << std::endl;
    }
    for (const auto &acc : vm->getInstMemoryAccess()) {
        std::ostringstream logStream;

        if (acc.type == MEMORY_READ) {
            logStream << "   mem[r]: 0x" << std::hex << acc.accessAddress << " size: " << acc.size
                      << " value: 0x" << acc.value;
        } else if (acc.type == MEMORY_WRITE) {
            logStream << "   mem[w]: 0x" << std::hex << acc.accessAddress << " size: " << acc.size
                      << " value: 0x" << acc.value;
        } else { // MEMORY_READ_WRITE
            logStream << "   mem[rw]: 0x" << std::hex << acc.accessAddress << " size: " << acc.size
                      << " value: 0x" << acc.value;
        }
        // 输出日志
        thiz->logbuf << logStream.str() << std::endl;
    }
    thiz->logbuf << std::endl;
    return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction showSyscall(QBDI::VM *vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data) {
    const QBDI::InstAnalysis *instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_INSTRUCTION);

    if (instAnalysis->mnemonic && strcasecmp(instAnalysis->mnemonic, "svc") == 0) {
        auto thiz = (class vm *) data;
        rword syscall_number = gprState->x8;
        const char *syscall_name = get_syscall_name(syscall_number);

        // 记录系统调用信息
        thiz->logbuf << "syscall " << syscall_name << "(x8=0x" << std::hex << syscall_number << ")\n";

        // 获取系统调用参数
        rword arg0 = gprState->x0;
        rword arg1 = gprState->x1;
        rword arg2 = gprState->x2;
        rword arg3 = gprState->x3;

        // 处理 openat 系统调用
        if (syscall_number == get_syscall_id_by_name("__NR_openat")) {
            // 读取 pathname（arg1）
            const char *pathname = reinterpret_cast<const char *>(arg1);
            char path_buffer[256] = {0};

            // 确保 `pathname` 是可读的
            if (safeReadMemory(arg1, reinterpret_cast<uint8_t *>(path_buffer), sizeof(path_buffer) - 1)) {
                pathname = path_buffer;  // 读取成功
            } else {
                pathname = "(invalid memory)";
            }

            // 打印并记录 openat 参数
            std::stringstream syscall_log;
            syscall_log << "syscall openat(dirfd=0x" << std::hex << arg0
                        << ", pathname=" << pathname
                        << ", flags=0x" << arg2
                        << ", mode=0x" << arg3 << ")\n";
            thiz->logbuf << syscall_log.str();      // 写入日志缓冲区
        }
    }

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

    cid = qvm.addCodeCB(QBDI::PREINST, showSyscall, this);
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