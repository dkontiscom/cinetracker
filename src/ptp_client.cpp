#include "ptp_client.h"

#include <t41-ptp.h>
#include <QNEthernet.h>

using namespace qindesign::network;

static l3PTP g_ptp(false, true, false);
static bool g_started = false;

int ptp_client_init() {
  EthernetIEEE1588.begin();

  // only one onLinkState slot exists in qnethernet dont register a 2nd one elsewhere
  Ethernet.onLinkState([](bool state) {
    Serial.printf("Ptp: link %s\n", state ? "up" : "down");
    if (state && !g_started) {
      g_ptp.begin();
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
