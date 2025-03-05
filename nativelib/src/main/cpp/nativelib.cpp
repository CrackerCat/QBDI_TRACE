#include <jni.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <dlfcn.h>
#include <fstream>
#include "vm.h"
#include "nativelib.h"
#include "HookUtils.h"
#include "HookInfo.h"

using namespace std;
#define HOOK_ADDR_DOBBY(func)  \
  DobbyHook((void*)func,  (void*) new_##func, (void**) &orig_##func); \


void aaa();

uint64_t get_tick_count64() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (ts.tv_sec*1000 + ts.tv_nsec/(1000*1000));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_f_nativelib_NativeLib_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    auto a = env->NewStringUTF("123123");
    env->DeleteLocalRef(a);


    long b = 0;
    while (true){
        b++;
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

        if ( b >= 1l){
            break;
        }
    }

    //aaa();
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
    DobbyInstrumentQBDI((void *) (add), vm_handle_add);
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

//    std::string data = get_data_path(gContext); // 获取日志文件的路径
    char* private_path = getPrivatePath();
    string p = private_path;
    p.append("trace_log.txt");
    out.open(p.c_str(), std::ios::out); // 打开或创建日志文件
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