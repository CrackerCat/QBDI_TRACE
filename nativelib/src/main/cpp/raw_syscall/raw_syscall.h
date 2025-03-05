

#ifndef HUNTER_RAW_SYSCALL_H
#define HUNTER_RAW_SYSCALL_H



#define INLINE __always_inline

#include <asm/unistd.h>
#include <sys/syscall.h>

struct syscall_mem_ptr_info{
    unsigned int size ;
    void* mem_ptr;
};
long raw_syscall(long __number, ...);
long raw_syscall_inline(long __number, ...);
syscall_mem_ptr_info getSyscallMemPtr();
void init_mem_raw_syscall();

#endif //HUNTER_RAW_SYSCALL_H