
#include "dobby/dobby_internal.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

#include "InterceptRouting/Routing/InstructionInstrumentQBDI/InstructionInstrumentRoutingQBDI.h"
#include "InterceptRouting/Routing/InstructionInstrumentQBDI/instrument_routing_handlerQBDI.h"

// create closure trampoline jump to prologue_routing_dispatch with the `entry_` data
void InstructionInstrumentRoutingQBDI::BuildRouting() {
  void *handler = (void *) instrument_routing_dispatchQBDI;
#if defined(__APPLE__) && defined(__arm64__)
  handler = pac_strip(handler);
#endif
  auto closure_trampoline = ClosureTrampoline::CreateClosureTrampoline(entry_, handler,fromQBDI);
  this->SetTrampolineTarget((addr_t)closure_trampoline->address);
  DEBUG_LOG("[closure trampoline] closure trampoline: %p, data: %p", closure_trampoline->address, entry_);

  // generate trampoline buffer, before `GenerateRelocatedCode`
  addr_t from = entry_->patched_addr;
#if defined(TARGET_ARCH_ARM)
  if (entry_->thumb_mode)
    from += 1;
#endif
  addr_t to = GetTrampolineTarget();
  GenerateTrampolineBuffer(from, to);
  // b lr
  //  DEBUG_LOG("b lr");
  //  GetTrampolineBuffer()->Emit(0xD65F03C0);
}

//void log_hex_format(uint8_t *buffer, uint32_t buffer_size) {
//  char output[1024] = {0};
//  for (int i = 0; i < buffer_size && i < sizeof(output); i++) {
//    snprintf(output + strlen(output), 3, "%02x ", *((uint8_t *)buffer + i));
//  }
//  DEBUG_LOG("%s", output);
//};

bool InstructionInstrumentRoutingQBDI::QBDIRelocatedCode(){
  uint32_t tramp_size = GetTrampolineBuffer()->GetBufferSize();
  origin_ = new CodeMemBlock(entry_->patched_addr, tramp_size);
  relocated_ = new CodeMemBlock();

  auto buffer = (void *)entry_->patched_addr;
#if defined(TARGET_ARCH_ARM)
  if (entry_->thumb_mode) {
    buffer = (void *)((addr_t)buffer + 1);
  }
#endif
  GenRelocateCodeAndBranch(buffer, origin_, relocated_);
  if (relocated_->size == 0) {
    ERROR_LOG("[insn relocate]] failed");
    return false;
  }

  // set the relocated instruction address
  entry_->relocated_addr = relocated_->addr;

  // save original prologue
  memcpy((void *)entry_->origin_insns, (void *)origin_->addr, origin_->size);
  entry_->origin_insn_size = origin_->size;

  // log
  DEBUG_LOG("[insn relocate] origin %p - %d", origin_->addr, origin_->size);
//  log_hex_format((uint8_t *)origin_->addr, origin_->size);

  DEBUG_LOG("[insn relocate] relocated %p - %d", relocated_->addr, relocated_->size);
//  log_hex_format((uint8_t *)relocated_->addr, relocated_->size);

  return true;

}

void InstructionInstrumentRoutingQBDI::DispatchRouting() {
  BuildRouting();

  // generate relocated code which size == trampoline size
  GenerateRelocatedCode();
}

#if 0
void *InstructionInstrumentRouting::GetTrampolineTarget() {
  return this->prologue_dispatch_bridge;
}
#endif
