#include "master.h"

#include "uart_protocol.h"

UartProtocol uartProtocol(Serial2);
RequestPacket  requestPkt;
ResponsePacket responsePkt;
UartStatus status;

static const UartAlgorithm ALGO_PREFERENCE[] = {
    ALGO_ASCON,
    ALGO_TINYJAMBU,
    ALGO_SCHWAEMM,
};
static const uint8_t ALGO_COUNT =
    sizeof(ALGO_PREFERENCE) / sizeof(ALGO_PREFERENCE[0]);

void request_slave() {
  status = uartProtocol.masterSendHi({ ALGO_PREFERENCE[1] });
  if (status == UART_ERR_NACK) {
    Serial.println("Slave does not support Ascon");
    return;
  } else if (status != UART_OK) {
    Serial.println("Failed to negotiate algorithm with slave");
    return;
  } else {
    Serial.println("Successfully negotiated algorithm with slave");
  }

  const char* demoData = "Hello, ESP32 UART protocol!";
  // const char* demoKey  = "secretkey123";
  // const char* demoNonce = "nonce123";

  uint8_t demoKeyBytes[24];
  uint8_t demoNonceBytes[24];
  for (int i = 0; i < 24; i++) {
    demoKeyBytes[i] = random(256);
    demoNonceBytes[i] = random(256);
  }

  // uint8_t demoKeyBytes[16];
  // uint8_t demoNonceBytes[12];
  // int i;
  // for (i = 0; i < 12; i++) {
  //   demoKeyBytes[i] = random(256);
  //   demoNonceBytes[i] = random(256);
  // }
  // for (; i < 16; i++) {
  //   demoKeyBytes[i] = random(256);
  // }

  requestPkt.algorithm = ALGO_PREFERENCE[2];
  requestPkt.dataSize  = (uint16_t)strlen(demoData);
  requestPkt.keySize   = (uint8_t)sizeof(demoKeyBytes);
  requestPkt.nonceSize = (uint8_t)sizeof(demoNonceBytes);
  requestPkt.data = (uint8_t*)demoData; // Point to demoData string
  // memcpy(requestPkt.data, demoData, requestPkt.dataSize);
  memcpy(requestPkt.key,  demoKeyBytes,  requestPkt.keySize);
  memcpy(requestPkt.nonce, demoNonceBytes, requestPkt.nonceSize);

  status = uartProtocol.masterSendRequest(requestPkt);
  if (status != UART_OK) {
    Serial.println("Failed to send request to slave");
    return;
  }
  else {
    Serial.println("Request sent to slave");
  }

  status = uartProtocol.masterReceiveResponse(responsePkt);
  if (status != UART_OK) {
    Serial.println("Failed to receive response from slave");
    return;
  }
  else {
    Serial.println("Received response from slave");
  }
}