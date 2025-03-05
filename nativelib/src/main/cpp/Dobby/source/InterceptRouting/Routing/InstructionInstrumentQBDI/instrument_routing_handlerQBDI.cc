#include "dobby/dobby_internal.h"

#include "InterceptRouting/Routing/InstructionInstrumentQBDI/InstructionInstrumentRoutingQBDI.h"
#include "InterceptRouting/Routing/InstructionInstrumentQBDI/instrument_routing_handlerQBDI.h"

#include "TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.h"

void instrument_forward_handlerQBDI(InterceptEntry *entry, DobbyRegisterContext *ctx) {
  auto routing = static_cast<InstructionInstrumentRoutingQBDI *>(entry->routing);
  if (routing->pre_handler) {
    auto handler = (dobby_instrument_callback_t_QBDI)routing->pre_handler;
    (*handler)((void *)entry->patched_addr, ctx,&(entry->relocated_addr));
  }

  // set prologue bridge next hop address as relocated instructions
  DEBUG_LOG("relocated_addr  ==> %p",entry->relocated_addr);
  set_routing_bridge_next_hop(ctx, (void *)entry->relocated_addr);
}

void instrument_routing_dispatchQBDI(InterceptEntry *entry, DobbyRegisterContext *ctx) {
  instrument_forward_handlerQBDI(entry, ctx);
}
