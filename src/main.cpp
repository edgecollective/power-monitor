// ProMicro NRF52840 power cycler — first step toward an external watchdog.
//
// Cycle: sleep SLEEP_MS with CYCLE_PIN low -> wake, CYCLE_PIN high for ON_MS -> repeat.
//
// "Sleep" here is nRF52 System ON idle: delay() in the Adafruit core is a FreeRTOS
// vTaskDelay, and with tickless idle the CPU sits in WFE with only the 32 kHz RTC
// running. GPIO outputs keep their level while asleep, which is what we need to
// hold a MOSFET gate. (System OFF is lower still, but it has no timer wake source —
// only a pin change or reset can wake it — so it can't run this cycle on its own.)

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>  // provides Serial (USB CDC) on nRF52840
#include "nrf_gpio.h"

#ifndef CYCLE_PIN
#define CYCLE_PIN 17  // P0.17
#endif
#ifndef SLEEP_MS
#define SLEEP_MS 10000
#endif
#ifndef ON_MS
#define ON_MS 10000
#endif
#ifndef MIRROR_LED
#define MIRROR_LED 0
#endif

static void log(const char* msg) {
  // Only talk when a USB host has the port open; on battery this costs nothing.
  if (Serial) {
    Serial.print(millis());
    Serial.print(" ms: ");
    Serial.println(msg);
    // Short lines sit in the USB buffer until a full packet accumulates; push them now.
    Serial.flush();
  }
}

static void setOutput(bool on) {
  if (on) nrf_gpio_pin_set(CYCLE_PIN);
  else    nrf_gpio_pin_clear(CYCLE_PIN);
#if MIRROR_LED
  digitalWrite(LED_BUILTIN, on ? LED_STATE_ON : !LED_STATE_ON);
#endif
}

// Sleep in short slices rather than one long delay(). With a single 10 s delay()
// USB failed to enumerate; a 500 ms delay loop did not. The extra wake-ups cost
// only a few µA on average.
#define SLEEP_SLICE_MS 500

static void sleepFor(uint32_t ms) {
  while (ms > 0) {
    uint32_t slice = ms < SLEEP_SLICE_MS ? ms : SLEEP_SLICE_MS;
    delay(slice);
    ms -= slice;

    // Send 'u' over USB serial to reboot into the UF2 bootloader (no double-tap needed).
    if (Serial.available() && Serial.read() == 'u') enterUf2Dfu();
  }
}

void setup() {
  // Drive the pin low immediately so the MOSFET starts in a known (off) state.
  nrf_gpio_cfg_output(CYCLE_PIN);
  setOutput(false);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, !LED_STATE_ON);

  Serial.begin(115200);
}

void loop() {
  setOutput(false);
  log("pin LOW, sleeping");
  sleepFor(SLEEP_MS);

  setOutput(true);
  log("pin HIGH");
  sleepFor(ON_MS);
}
