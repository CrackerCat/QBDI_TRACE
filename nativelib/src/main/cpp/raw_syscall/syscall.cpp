

#include "raw_syscall.h"
#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>



#define UNLIKELY(x) __builtin_expect(!!(x), 0)


typedef long (*syscall_func)(long number, ...);

void* syscall_mem ;
static unsigned long gMemcrc = 0;
syscall_mem_ptr_info ptr_info = {0};

#define syscall_mod_mem 1
#define syscall_mod_inline 2


#if defined(__aarch64__)
//"MOV X8, X0\n\t"
//"MOV X0, X1\n\t"
//"MOV X1, X2\n\t"
//"MOV X2, X3\n\t"
//"MOV X3, X4\n\t"
//"MOV X4, X5\n\t"
//"MOV X5, X6\n\t"
//"SVC 0\n\t"
const unsigned char syscall_code[] = {
        0xE8, 0x03, 0x00, 0xAA,//MOV X8, X0
        0xE0, 0x03, 0x01, 0xAA,//MOV X0, X1
        0xE1, 0x03, 0x02, 0xAA,//MOV X1, X2
        0xE2, 0x03, 0x03, 0xAA, //MOV X2, X3
        0xE3, 0x03, 0x04, 0xAA,//MOV X3, X4
        0xE4, 0x03, 0x05, 0xAA,//MOV X4, X5
        0xE5, 0x03, 0x06, 0xAA, //MOV X5, X6
        0x01, 0x00, 0x00, 0xD4, //SVC 0
        0xC0, 0x03, 0x5F, 0xD6//RET
};

#else
//        MOV             R12, SP
//        STMFD           SP!, {R4-R7}
//        MOV             R7, R0
//        MOV             R0, R1
//        MOV             R1, R2
//        MOV             R2, R3
//        LDMIA           R12, {R3-R6}
//        SVC             0
//        LDMFD           SP!, {R4-R7}
//        mov             pc, lr
const unsigned char syscall_code[] = {
    0x0D,0xC0,0xA0,0xE1,//MOV             R12, SP
    0xF0,0x00,0x2D,0xE9,//STMFD           SP!, {R4-R7}
    0x00,0x70,0xA0,0xE1,//MOV             R7, R0
    0x01,0x00,0xA0,0xE1,//MOV             R0, R1
    0x02,0x10,0xA0,0xE1,//MOV             R1, R2
    0x03,0x20,0xA0,0xE1,//MOV             R2, R3
    0x78,0x00,0x9C,0xE8,//LDMIA           R12, {R3-R6}
    0x00,0x00,0x00,0xEF,//SVC             0
    0x0F,0x00,0xB0,0xE8,//LDMFD           SP!, {R4-R7}
    0x0E,0xF0,0xA0,0xE1,//mov             pc, lr
};

#endif

void print_hex(void *ptr, size_t length) {
    unsigned char *bytes = (unsigned char *) ptr;
    char *output = (char *) malloc(length * 3 + 1);
    if (!output) {
        // 内存分配失败
        LOGE("Memory allocation failed");
        return;
    }
    char *current = output;
    for (size_t i = 0; i < length; ++i) {
        current += sprintf(current, "%02X ", bytes[i]);
    }
//    LOG(ERROR) << length << " code -> " << output;
    free(output);
}

INLINE long raw_syscall_inline(long __number, ...) {
    long result;
#if defined(__aarch64__)
    __asm__ (
            "MOV X8, X0\n\t"
            "MOV X0, X1\n\t"
            "MOV X1, X2\n\t"
            "MOV X2, X3\n\t"
            "MOV X3, X4\n\t"
            "MOV X4, X5\n\t"
            "MOV X5, X6\n\t"
            "SVC 0\n\t"
            "MOV %0, X0\n\t"
            : "=r" (result)
            :
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x8" // 破坏列表
            );
#else
    __asm__ (
        "MOV R12, SP\n\t"
        "STMFD SP!, {R4-R7}\n\t"
        "MOV R7, R0\n\t"
        "MOV R0, R1\n\t"
        "MOV R1, R2\n\t"
        "MOV R2, R3\n\t"
        "LDMIA R12, {R3-R6}\n\t"
        "SVC 0\n\t"
        "LDMFD SP!, {R4-R7}\n\t"
        "MOV %0, R0\n\t"
        : "=r" (result)
        :
        : "r0", "r1", "r2", "r3", "r4", "r7", "r12"
    );
#endif
    return result;
}


syscall_mem_ptr_info getSyscallMemPtr() {
    return ptr_info;
}

static inline unsigned long checksum(void *buffer, size_t len) {
    if (buffer == nullptr) {
        return 0;
    }
    unsigned long seed = 0;
    auto *buf = (uint8_t *) buffer;
    for (size_t i = 0; i < len; ++i) {
        seed += (unsigned long) (buf[i]);
    }
    return seed;
}


inline void init_mem_raw_syscall() {

    if (syscall_mem == nullptr) {
#if defined(__aarch64__)
        syscall_mem = (char *) syscall(__NR_mmap, nullptr, sizeof(syscall_code),
                                       PROT_READ | PROT_WRITE | PROT_EXEC,
                                       MAP_ANONYMOUS | MAP_PRIVATE,
                                       -1, 0);
#else
        //arm 32 only mmap
        syscall_mem = (char*)syscall(__NR_mmap2,nullptr, sizeof(syscall_code),
                                         PROT_READ | PROT_WRITE | PROT_EXEC,
                                         MAP_ANONYMOUS | MAP_PRIVATE,
                                         -1, 0);
#endif
        if (syscall_mem == MAP_FAILED) {
            return ;
        }
        std::memset((void*)syscall_mem, 0, sizeof(syscall_code));
        std::memcpy((void*)syscall_mem, syscall_code, sizeof(syscall_code));
        //get syscall_code crc
        gMemcrc = checksum((void *) syscall_mem, sizeof(syscall_code));
        //not write
        mprotect((void*)syscall_mem, sizeof(syscall_code), PROT_READ | PROT_EXEC);
        ptr_info.mem_ptr = syscall_mem;
        ptr_info.size = sizeof(syscall_code);
//        LOGE(">>>>>>>> init_mem_raw_syscall %s %lu %p",getprogname(),sizeof(syscall_code),syscall_mem)
    }
    return ;
}



long raw_syscall(long __number, ...) {
    syscall_func func;
    unsigned short syscall_mod;

    //使用mmap 动态生成 syscall
      init_mem_raw_syscall();
   // LOGE("syscall_mem : %d" ,syscall_mem);
    if (syscall_mem) {
     //   LOGE("syscall_mod_mem");
        func = (syscall_func) syscall_mem;
        syscall_mod = syscall_mod_mem;
    } else{
    //    LOGE("raw_syscall_inline");
        func = (syscall_func) raw_syscall_inline;
        syscall_mod = syscall_mod_inline;
    }

    func = (syscall_func) raw_syscall_inline;
    syscall_mod = syscall_mod_inline;

    if (syscall_mod == syscall_mod_mem) {
        if (UNLIKELY(gMemcrc != checksum((void *) func, sizeof(syscall_code)))) {
            //mem mod crc error
            exit(11);
        }
    }

    //syscall args max 7
    void *arg[7];
    va_list list;
    va_start(list, __number);
    for (auto &i: arg) {
        i = va_arg(list, void *);
    }
    va_end(list);
    if (syscall_mod == syscall_mod_mem) {
        munmap(syscall_mem, sizeof(syscall_code));
        syscall_mem = nullptr;
    }
    return func(__number, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6]);
}