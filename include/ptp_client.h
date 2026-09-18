#pragma once

#include <Arduino.h>

// >5 is the libs own convention for locked not something we picked
#define PTP_LOCK_THRESHOLD 5
// no sync exchange for this long means the master is gone, not a hiccup.
// the lib itself treats >1100ms as stale but that assumes a 1s sync
// interval, masters go up to 2s, so this sits well clear of that
#define PTP_STALE_MS 5000

typedef void (*PtpInterruptCallback)();

int ptp_client_init(double frame_duration_ns, int64_t jump_threshold_ns,
                     double rate_limit_per_update, double clock_update_rate_hz,
                     PtpInterruptCallback interrupt_blocker, PtpInterruptCallback update_interrupt);
void ptp_client_process();
bool ptp_client_locked();
// grandmaster clock identity (8 bytes, eui-64) + ptp domain from the last
// Announce seen, false until one has arrived, used for the aes67 sdp
bool ptp_client_master_identity(uint8_t id[8], uint8_t *domain);
