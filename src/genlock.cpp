#include "genlock.h"

#define TRISYNC_MHZ GENLOCK_MHZ
#define TRISYNC_FPS GENLOCK_FPS
#include <t41-tri-sync-ptp.h>

#include "ptp_client.h"

int genlock_init() {
  trisync::begin(GENLOCK_INTERRUPT_CHANNEL, GENLOCK_PIN_BIT0, GENLOCK_PIN_BIT1);

  attachInterruptVector(IRQ_ENET_TIMER, trisync::pulse);
  NVIC_ENABLE_IRQ(IRQ_ENET_TIMER);

  return ptp_client_init(trisync::FRAMEDURATION, trisync::DEFAULTJUMPTHRESHOLD, 800, 4,
                          &trisync::block_incomming_interrupt, &trisync::update_interrupt_counter);
}

bool genlock_locked() {
  return ptp_client_locked();
}
