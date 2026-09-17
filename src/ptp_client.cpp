#include "ptp_client.h"

#include <t41-ptp.h>
#include <QNEthernet.h>

using namespace qindesign::network;

static l3PTP g_ptp(false, true, false);
static bool g_started = false;

static double g_frame_duration;
static int64_t g_jump_threshold;
static double g_rate_limit;
static double g_clock_update_rate;
static PtpInterruptCallback g_blocker;
static PtpInterruptCallback g_updater;

int ptp_client_init(double frame_duration_ns, int64_t jump_threshold_ns,
                     double rate_limit_per_update, double clock_update_rate_hz,
                     PtpInterruptCallback interrupt_blocker, PtpInterruptCallback update_interrupt) {
  g_frame_duration = frame_duration_ns;
  g_jump_threshold = jump_threshold_ns;
  g_rate_limit = rate_limit_per_update;
  g_clock_update_rate = clock_update_rate_hz;
  g_blocker = interrupt_blocker;
  g_updater = update_interrupt;

  EthernetIEEE1588.begin();

  // only one onLinkState slot exists in qnethernet dont register a 2nd one elsewhere
  Ethernet.onLinkState([](bool state) {
    Serial.printf("Ptp: link %s\n", state ? "up" : "down");
    if (state && !g_started) {
      g_ptp.begin(g_frame_duration, g_jump_threshold, g_rate_limit, g_clock_update_rate, g_blocker,
                  g_updater, false);
      g_started = true;
    }
  });

  return 0;
}

void ptp_client_process() {
  if (g_started) g_ptp.update();
}

bool ptp_client_locked() {
  return g_started && g_ptp.getLockCount() > PTP_LOCK_THRESHOLD;
}

int64_t ptp_client_offset_ns() {
  return g_started ? g_ptp.getOffset() : 0;
}
