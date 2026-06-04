#pragma once

#include "uart_protocol.h"

bool wait_for_master();
bool respond_to_master();
bool isAlgorithmSupported(UartAlgorithm algo);