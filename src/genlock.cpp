#include "genlock.h"

#define TRISYNC_MHZ GENLOCK_MHZ
#include <t41-tri-sync-ptp.h>

#include "ptp_client.h"

using trisync::TriSync;

static uint8_t g_active_fps = 0;

bool genlock_fps_supported(uint8_t fps) {
  return fps == 24 || fps == 25 || fps == 30 || fps == 50 || fps == 60;
}

// one macro instead of the same block repeated per rate so a copy paste
// slip cant end up wiring the wrong templates fps to a given case.
// priority 0 is the top: the sync pulse edge is the one thing here that
// must not be delayed by anyone else's handler (i2s dma, ethernet rx, the
// encoder sampling timer, all at the teensy default 128). ptp is fine
// below it - its timestamps are captured in hardware by the mac, rx isr
// latency doesnt move them. noInterrupts() still masks it, its primask
#define GENLOCK_CASE(RATE)                                                                  \
  case RATE: {                                                                              \
    TriSync<RATE>::begin(GENLOCK_INTERRUPT_CHANNEL, GENLOCK_PIN_BIT0, GENLOCK_PIN_BIT1);     \
    attachInterruptVector(IRQ_ENET_TIMER, TriSync<RATE>::pulse);                            \
    NVIC_SET_PRIORITY(IRQ_ENET_TIMER, 0);                                                   \
    NVIC_ENABLE_IRQ(IRQ_ENET_TIMER);                                                        \
    result = ptp_client_init(TriSync<RATE>::FRAMEDURATION, TriSync<RATE>::DEFAULTJUMPTHRESHOLD, \
                              800, 4, &TriSync<RATE>::block_incomming_interrupt,             \
                              &TriSync<RATE>::update_interrupt_counter);                    \
    break;                                                                                  \
  }

int genlock_init(uint8_t fps) {
  if (!genlock_fps_supported(fps)) fps = GENLOCK_DEFAULT_FPS;
  g_active_fps = fps;

  int result = -1;
  switch (fps) {
    GENLOCK_CASE(24)
    GENLOCK_CASE(25)
    GENLOCK_CASE(30)
    GENLOCK_CASE(50)
    GENLOCK_CASE(60)
    default:
      break;
  }

  return result;
}

#undef GENLOCK_CASE

bool genlock_locked() {
  return ptp_client_locked();
}

uint8_t genlock_get_fps() {
  return g_active_fps;
}
