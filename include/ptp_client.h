#pragma once

#include <Arduino.h>

// >5 is the libs own convention for locked not something we picked
#define PTP_LOCK_THRESHOLD 5

int ptp_client_init();
void ptp_client_process();
bool ptp_client_locked();
int64_t ptp_client_offset_ns();
