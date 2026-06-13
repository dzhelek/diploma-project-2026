#pragma once

#include <Arduino.h>

// Slave debug logging.
//
// On slaves whose ONLY UART is also the protocol link (ESP8266 / Arduino Nano),
// a console print would inject bytes into the protocol stream and corrupt it — so
// these macros expand to nothing there. Because the arguments don't appear in the
// empty expansion, the String formatting isn't even built (zero overhead).
//
// On slaves with a separate console — ESP32 (Serial2 link, Serial console) and
// Pico (Serial1 link, USB Serial console) — the prints go to that console.
#if defined(SLAVE_ESP32) || defined(SLAVE_RASPBERRY_PI_PICO)
  #define SLAVE_DBG(...)    Serial.print(__VA_ARGS__)
  #define SLAVE_DBGLN(...)  Serial.println(__VA_ARGS__)
#else
  #define SLAVE_DBG(...)    ((void)0)
  #define SLAVE_DBGLN(...)  ((void)0)
#endif
