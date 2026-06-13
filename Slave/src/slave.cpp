#include "slave.h"

#include "crypto_aead.h"
#include "slave_debug.h"

UartProtocol uartProtocol(LINK_SERIAL);  // LINK_SERIAL is defined per-board in slave.h
UartStatus status;
HiPacket hiPkt;

UartAlgorithm SUPPORTED_ALGOS[] = {
    ALGO_ASCON,
    ALGO_TINYJAMBU,
    ALGO_SCHWAEMM,
};
const uint8_t SUPPORTED_ALGO_COUNT =
    sizeof(SUPPORTED_ALGOS) / sizeof(SUPPORTED_ALGOS[0]);

bool wait_for_master(RequestPacket& pkt) {

  SLAVE_DBGLN("Waiting for Hi");
  status = uartProtocol.slaveReceiveHi(hiPkt, SUPPORTED_ALGOS, SUPPORTED_ALGO_COUNT);
  if (status == UART_ERR_NACK) {
    SLAVE_DBGLN("Master requested unsupported algorithm");
    return false;
  } else if (status != UART_OK) {
    SLAVE_DBGLN("Failed to receive Hi from master, status: " + String(status));
    return false;
  } else {
    SLAVE_DBG("Received Hi from master, algorithm: ");
    SLAVE_DBGLN(hiPkt.algorithm);
  }

  status = uartProtocol.slaveReceiveRequest(pkt);
  if (status != UART_OK) {
    SLAVE_DBGLN("Failed to receive request from master, status:" + String(status));
    return false;
  } else {
    SLAVE_DBGLN("Received request from master");
  }
  return true;
}

bool respond_to_master(ResponsePacket& pkt) {
  status = uartProtocol.slaveSendResponse(pkt);
  if (status != UART_OK) {
    SLAVE_DBGLN("Failed to send response to master, status: " + String(status));
    return false;
  } else {
    SLAVE_DBGLN("Sent response to master");
    return true;
  }
}

/** Return true if this slave supports @p algo. */
bool isAlgorithmSupported(UartAlgorithm algo)
{
    for (uint8_t i = 0; i < SUPPORTED_ALGO_COUNT; ++i) {
        if (SUPPORTED_ALGOS[i] == algo) return true;
    }
    return false;
}
