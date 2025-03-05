//
// Created by fang on 23-12-19.
//

#ifndef QBDIRECORDER_VM_H
#define QBDIRECORDER_VM_H
#include "QBDI.h"
#include "QBDI/State.h"
#include "dobby.h"
#include "mongoose.h"
//#include "socketUtils.h"
#include "fileRecord.h"
#include "logger.h"
#include "json.hpp"
#include "jsonbean.h"

#include <sstream>

#define STACK_SIZE  0x800000
void syn_regs(DobbyRegisterContext *ctx,QBDI::GPRState *state,bool D2Q);
void syn_regs(DobbyRegisterContext *ctx, QBDI::GPRState *state);
class vm {

public:
    QBDI::VM init(void* address);
    std::stringstream logbuf;

private:

};


//void vm_handle(void* address, DobbyRegisterContext *ctx, addr_t *relocated_addr) {
//    LOGE("vm address %p ", address);
//    DobbyDestroy(address);
//    auto vm_ = new vm();
//    auto qvm = vm_->init(address);
//    auto state = qvm.getGPRState();
//    QBDI::rword retval;
//    syn_regs(ctx, state, true);
//
//    uint8_t *fakestack;
//    QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);
//
//    qvm.call(nullptr, (uint64_t) address);
//    syn_regs(ctx, state, false);
//    QBDI::alignedFree(fakestack);
//}

//void setup_vm_instrumentation(void* address) {
//    DobbyInstrumentQBDI(address, vm_handle);
//}

#endif //QBDIRECORDER_VM_H
