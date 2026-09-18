#include "reset_button.h"

#include "storage.h"

static bool g_pressed = false;
static uint32_t g_press_start = 0;

void reset_button_init() {
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
}

// active low has to be held the full threshold a tap does nothing so
// bumping it on set cant wipe a device mid shoot
void reset_button_process() {
  bool down = digitalRead(RESET_BUTTON_PIN) == LOW;

  if (down && !g_pressed) {
    g_pressed = true;
    g_press_start = millis();
  } else if (!down) {
    g_pressed = false;
  } else if (g_pressed && millis() - g_press_start >= RESET_BUTTON_HOLD_MS) {
    Serial.println("Reset button: hold threshold reached doing factory reset");
    storage_factory_reset(0xDEADBEEF);
    // quickFormat already wipes the boot flag along with everything else
    // this just makes the intent explicit instead of relying on that
    storage_set_clean_shutdown(true);
    delay(100);
    // same magic reset word handle_reboot() in web_interface cpp uses
    SCB_AIRCR = 0x05FA0004;
  }
}
