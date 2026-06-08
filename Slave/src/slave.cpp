#include "slave.h"

#include "crypto_aead.h"

#ifdef SLAVE_ESP32
UartProtocol uartProtocol(Serial2);
#endif
#ifdef SLAVE_ESP_12
UartProtocol uartProtocol(Serial);
#endif
#ifdef SLAVE_RASPBERRY_PI_PICO
UartProtocol uartProtocol(Serial1);
#endif
#ifdef SLAVE_ARDUINO_NANO
UartProtocol uartProtocol(Serial);
#endif
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

  Serial.println("Waiting for Hi");
  status = uartProtocol.slaveReceiveHi(hiPkt, SUPPORTED_ALGOS, SUPPORTED_ALGO_COUNT);
  if (status == UART_ERR_NACK) {
    Serial.println("Master requested unsupported algorithm");
    return false;
  } else if (status != UART_OK) {
    Serial.println("Failed to receive Hi from master");
    return false;
  } else {
    Serial.print("Received Hi from master, algorithm: ");
    Serial.println(hiPkt.algorithm);
  }

  status = uartProtocol.slaveReceiveRequest(pkt);
  if (status != UART_OK) {
    Serial.println("Failed to receive request from master");
    return false;
  } else {
    Serial.println("Received request from master");
  }
  return true;
}

bool respond_to_master(ResponsePacket& pkt) {
  status = uartProtocol.slaveSendResponse(pkt);
  if (status != UART_OK) {
    Serial.println("Failed to send response to master");
    return false;
  } else {
    Serial.println("Sent response to master");
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