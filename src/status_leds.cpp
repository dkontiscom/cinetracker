#include "status_leds.h"

#include "genlock.h"
#include "web_interface.h"

void status_leds_init() {
  pinMode(STATUS_LED_SYNC_PIN, OUTPUT);
  pinMode(STATUS_LED_STREAM_PIN, OUTPUT);
  digitalWrite(STATUS_LED_SYNC_PIN, LOW);
  digitalWrite(STATUS_LED_STREAM_PIN, LOW);
}

// led1 genlock/ptp locked led2 freed actively streaming plain reads of
// whats already tracked elsewhere not a new combined status
void status_leds_process() {
  digitalWrite(STATUS_LED_SYNC_PIN, genlock_locked() ? HIGH : LOW);
  digitalWrite(STATUS_LED_STREAM_PIN, web_is_streaming() ? HIGH : LOW);
}
