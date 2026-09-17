#pragma once

#include <Arduino.h>

// >5 is the libs own convention for locked not something we picked
#define PTP_LOCK_THRESHOLD 5

typedef void (*PtpInterruptCallback)();

int ptp_client_init(double frame_duration_ns, int64_t jump_threshold_ns,
                     double rate_limit_per_update, double clock_update_rate_hz,
                     PtpInterruptCallback interrupt_blocker, PtpInterruptCallback update_interrupt);
void ptp_client_process();
bool ptp_client_locked();
int64_t ptp_client_offset_ns();
