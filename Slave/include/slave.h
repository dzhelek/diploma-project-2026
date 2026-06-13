#pragma once

#include "uart_protocol.h"

// Single source of truth for which UART is the master<->slave link on each board.
// Both slave.cpp (constructing UartProtocol) and main.cpp's loop() use it, so they
// can't drift — that drift is exactly why the Pico spun on an unconnected Serial2.
#if defined(SLAVE_ESP32)
  #define LINK_SERIAL Serial2
#elif defined(SLAVE_ESP_12)
  #define LINK_SERIAL Serial
#elif defined(SLAVE_RASPBERRY_PI_PICO)
  #define LINK_SERIAL Serial1
#elif defined(SLAVE_ARDUINO_NANO)
  #define LINK_SERIAL Serial
#endif

bool wait_for_master(RequestPacket&);
bool respond_to_master(ResponsePacket&);
bool isAlgorithmSupported(UartAlgorithm);
