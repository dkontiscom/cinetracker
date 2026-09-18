#include "ptp_client.h"

#include <t41-ptp.h>
#include <QNEthernet.h>

using namespace qindesign::network;

static l3PTP g_ptp(false, true, false);
static bool g_started = false;
static bool g_link_up = false;

static double g_frame_duration;
static int64_t g_jump_threshold;
static double g_rate_limit;
static double g_clock_update_rate;
static PtpInterruptCallback g_blocker;
static PtpInterruptCallback g_updater;

static void on_link(bool state) {
  Serial.printf("Ptp: link %s\n", state ? "up" : "down");
  g_link_up = state;
  if (state && !g_started) {
    g_ptp.begin(g_frame_duration, g_jump_threshold, g_rate_limit, g_clock_update_rate, g_blocker,
                g_updater, false);
    g_started = true;
  }
}

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
  Ethernet.onLinkState(on_link);

  // the callback is change-only, qnethernet just stores the pointer and
  // never replays the current state. network_stack_init runs before us
  // and blocks in a 10s dhcp wait that services the link, so with a cable
  // plugged at boot the up edge fires before anyone is listening and ptp
  // never started - genlock never locked, only an unplug/replug woke it.
  // static ip was a race not a guarantee. so check now, once, by hand
  on_link(Ethernet.linkState());

  return 0;
}

void ptp_client_process() {
  if (g_started) g_ptp.update();
}

// lockcount alone froze at its last value the moment syncs stopped, so a
// pulled cable or a vanished master read as locked forever - the lock led
// stayed on and the genlock status kept saying synced. link state makes
// a cable pull show instantly, the staleness check catches a master that
// went away with the link still up
bool ptp_client_locked() {
  return g_started && g_link_up && g_ptp.getLockCount() > PTP_LOCK_THRESHOLD &&
         g_ptp.msSinceUpdate() < PTP_STALE_MS;
}

int64_t ptp_client_offset_ns() {
  return g_started ? g_ptp.getOffset() : 0;
}

bool ptp_client_master_identity(uint8_t id[8], uint8_t *domain) {
  if (!g_started || !domain) return false;
  return g_ptp.getMasterIdentity(id, *domain);
}
