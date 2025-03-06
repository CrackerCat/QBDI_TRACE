#include <jni.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <dlfcn.h>
#include <fstream>

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/system_properties.h>

#include "vm.h"
#include "nativelib.h"
#include "HookUtils.h"
#include "HookInfo.h"
#include "mylibc.h"


using namespace std;
#define HOOK_ADDR_DOBBY(func)  \
  DobbyHook((void*)func,  (void*) new_##func, (void**) &orig_##func); \


void aaa();

uint64_t get_tick_count64() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (ts.tv_sec*1000 + ts.tv_nsec/(1000*1000));
}


// 线程函数
void* thread_function(void* arg) {
    LOGE("Thread is running...");
    pthread_exit(nullptr);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_f_nativelib_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    std::string hello = "Hello from C++";

    // 创建线程
    pthread_t thread;
    if (pthread_create(&thread, nullptr, thread_function, nullptr) == 0) {
        LOGE("Thread created successfully!");
    } else {
        LOGE("Failed to create thread");
    }

    // 读取 /proc/self/maps
    int fd = my_open("/proc/self/maps", O_RDONLY, 0);  // 替换 open() 为 my_open()

    if (fd >= 0) {
        char buffer[256];
        ssize_t bytesRead = my_read(fd, buffer, sizeof(buffer) - 1);  // 替换 read() 为 my_read()

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            LOGE("Maps: %s", buffer);
        }

        close(fd);  // 这里的 close() 仍然可以使用标准库的 close()
    } else {
        LOGE("Failed to open /proc/self/maps");
    }


    // 读取 system property
    char prop_value[PROP_VALUE_MAX];
    if (__system_property_get("ro.build.version.release", prop_value) > 0) {
        LOGE("Android Version: %s", prop_value);
    } else {
        LOGE("Failed to read system property");
    }

    // 原有代码逻辑
    auto a = env->NewStringUTF("123123");
    env->DeleteLocalRef(a);

    long b = 0;
    while (true) {
        __atomic_fetch_add(&b, 1, __ATOMIC_SEQ_CST);  // 原子加法操作

        char text[100] = "aasddsadsa";
        int shift = 15;
        int i;

        for (i = 0; i < strlen(text); i++) {
            if (text[i] >= 'A' && text[i] <= 'Z') {
                text[i] = 'A' + ((text[i] - 'A' + shift) % 26);
            } else if (text[i] >= 'a' && text[i] <= 'z') {
                text[i] = 'a' + ((text[i] - 'a' + shift) % 26);
            }
        }

        int atomic_val = 42;  // 普通整数变量

        // 使用 GCC 原子操作
        int prev_and = __atomic_fetch_and(&atomic_val, 0x0F, __ATOMIC_SEQ_CST);
        int prev_or = __atomic_fetch_or(&atomic_val, 0xF0, __ATOMIC_SEQ_CST);
        int prev_xor = __atomic_fetch_xor(&atomic_val, 0xFF, __ATOMIC_SEQ_CST);

        std::cout << "Previous AND value: " << prev_and << "\n";
        std::cout << "Previous OR value: " << prev_or << "\n";
        std::cout << "Previous XOR value: " << prev_xor << "\n";

        if (b >= 1L) {
            break;
        }
        std::atomic<long> counter(0);


    }

    char text[100] = "aasddsadsa";
    int shift = 15;
    for (int i = 0; i < strlen(text); i++) {
        char old_val, new_val;

        do {
            old_val = text[i];

            if (old_val >= 'A' && old_val <= 'Z') {
                new_val = 'A' + ((old_val - 'A' + shift) % 26);
            } else if (old_val >= 'a' && old_val <= 'z') {
                new_val = 'a' + ((old_val - 'a' + shift) % 26);
            } else {
                break;  // 不是字母则跳过
            }

            // CAS（比较并交换）确保多线程环境下的安全修改
        } while (!std::atomic_compare_exchange_weak(
                reinterpret_cast<std::atomic<char>*>(&text[i]), &old_val, new_val));
    }

    // 线程等待
    pthread_join(thread, nullptr);

    return env->NewStringUTF(hello.c_str());
}


int sub(int a, int b) {
    return a + b;
}

int add(int a, int b) {
    int c = sub(5, 6);
    return a + b + c;
}



//void add_handle2(void* address,DobbyRegisterContext *ctx,addr_t * relocated_addr){
//    LOGE("address %p ",address);
//    LOGE("Java_com_f_nativelib_NativeLib_stringFromJNI %p ,lr %lx ",Java_com_f_nativelib_NativeLib_stringFromJNI,ctx->lr);
//    LOGE("%lx %lx",ctx->dmmpy_0,ctx->dmmpy_1);
//    LOGE("relocated_addr %p",relocated_addr);
//    DobbyDestroy(address);
//    auto vm_ = new vm();
//    auto qvm = vm_->init(address);
//    auto state = qvm.getGPRState();
//    QBDI::rword retval;
//    syn_regs(ctx,state, true);
//
//    qvm.call(&retval, (uint64_t) address);
//    syn_regs(ctx,state, false);
//    DobbyInstrumentQBDI(address,add_handle);
//}




void vm_handle_add(void* address, DobbyRegisterContext *ctx, addr_t *relocated_addr) {
    LOGE("VM_BEFORE %lx", ctx->general.x[0]);
    LOGE("VM_BEFORE %lx", ctx->general.x[1]);
    LOGE("vm address %p ", address);
    DobbyDestroy(address);
    auto vm_ = new vm();
    auto qvm = vm_->init(address);
    auto state = qvm.getGPRState();
    QBDI::rword retval;
    syn_regs(ctx, state, true);

    uint8_t *fakestack;
    QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);


    qvm.call(nullptr, (uint64_t) address);
    syn_regs(ctx, state, false);
    QBDI::alignedFree(fakestack);
    LOGE("VM_AFTER %lx", ctx->general.x[0]);
}


void aaa() {
//    DobbyInstrumentQBDI((void *) (add), vm_handle_add);
    LOGE("%d", add(1, 2));
    LOGE("%d", add(1, 2));
    LOGE("%d", add(1, 2));
}


void init() {
    linkerHandler_init();
}

JavaVM *mVm;
JNIEnv *mEnv;

jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reversed) {
    LOGE("JNI_ONLOAD start");
    mVm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        mEnv = env;

        //init();
        return JNI_VERSION_1_6;
    }
    return JNI_ERR;
}



void vm_handle_sig1(void* address, DobbyRegisterContext *ctx, addr_t *relocated_addr) {
    uint64_t now = get_tick_count64();
    size_t number = 1;

    LOGE("vm address %p ", address);
    LOGE("ctx->lr %zx",ctx->lr);
    DobbyDestroy(address);
    auto vm_ = new vm();
    auto qvm = vm_->init(address);
    auto state = qvm.getGPRState();
    QBDI::rword retval;
    syn_regs(ctx, state, true);

    uint8_t *fakestack;
    QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);

    qvm.call(nullptr, (uint64_t) address);
    syn_regs(ctx, state, false);
    QBDI::alignedFree(fakestack);

    // 将虚拟机的日志数据写入文件
    std::ofstream out;
    string private_path = getPrivatePath();

    private_path.append("trace_log.txt");
    out.open(private_path.c_str(), std::ios::out); // 打开或创建日志文件
    out << vm_->logbuf.str(); // 将虚拟机日志缓冲区的内容写入文件
    out.close(); // 关闭文件

    // 记录并输出函数执行时间
    LOGD("Read %ld times cost = %lfs\n", number, (double)(get_tick_count64() - now) / 1000);
}



extern "C" void _init(void) {
    LOGE("getPrivatePath %s", getPrivatePath());
    //aaa();
    init();

}

__unused __attribute__((constructor)) void init_main() {
    // add hook
    DobbyInstrumentQBDI((void*)Java_com_f_nativelib_NativeLib_stringFromJNI, vm_handle_sig1);
}