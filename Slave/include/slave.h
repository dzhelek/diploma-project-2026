#pragma once

#include "uart_protocol.h"

bool wait_for_master(RequestPacket&);
bool respond_to_master(ResponsePacket&);
bool isAlgorithmSupported(UartAlgorithm);