#include <Arduino.h>

#include "slave.h"
#include "algorithm_interface.h"

void setup_wifi();

uint32_t startMs, elapsed;
IAlgorithm* algo;
AlgoResult result;
AlgoStatus status_algo;

extern RequestPacket requestPkt;
extern ResponsePacket responsePkt;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX, TX

  Serial.println("Setup complete");

}

void loop() {
  while(!wait_for_master()) delay(100);

  algo = AlgorithmFactory(requestPkt.algorithm);

  if (requestPkt.keySize != algo->keySize()) {
    Serial.println("Invalid key size in request");
    return;
  }
  if (requestPkt.nonceSize != algo->nonceSize()) {
    Serial.println("Invalid nonce size in request");
    return;
  }

  startMs = millis();
  status_algo = algo->encrypt(
      requestPkt.data, requestPkt.dataSize,
      requestPkt.key,  requestPkt.keySize,
      requestPkt.nonce, requestPkt.nonceSize,
      nullptr, 0, // No associated data
      result
  );
  elapsed = millis() - startMs;

  responsePkt.timeMs = elapsed > 0xFFFF ? 0xFFFF : (uint16_t)elapsed;
  responsePkt.dataSize = result.outputSize;
  memcpy(responsePkt.data, result.output, result.outputSize);

  respond_to_master();
}
