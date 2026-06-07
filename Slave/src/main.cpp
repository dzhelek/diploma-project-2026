#include <Arduino.h>

#include "slave.h"
#include "algorithm_interface.h"

void setup_wifi();

uint32_t startMs, elapsed;
IAlgorithm* algo;
AlgoStatus status_algo;

RequestPacket requestPkt;
ResponsePacket responsePkt;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX, TX

  Serial.println("Setup complete");

}

void loop() {
  while(!wait_for_master(requestPkt)) delay(100);

  algo = AlgorithmFactory(requestPkt.algorithm);

  startMs = millis();

  size_t responsePktDataSize = requestPkt.dataSize + algo->tagSize();
  responsePkt.data = new uint8_t[responsePktDataSize]; // Allocate buffer for ciphertext + tag
  status_algo = algo->encrypt(
      requestPkt.data, requestPkt.dataSize,
      requestPkt.key,  requestPkt.keySize,
      requestPkt.nonce, requestPkt.nonceSize,
      nullptr, 0, // No associated data
      responsePkt.data, &responsePktDataSize
  );

  elapsed = millis() - startMs;

  responsePkt.dataSize = responsePktDataSize;
  responsePkt.timeMs = elapsed > 0xFFFF ? 0xFFFF : (uint16_t)elapsed;

  respond_to_master(responsePkt);

  delete[] responsePkt.data; // Clean up ciphertext buffer
  delete[] requestPkt.data;  // Clean up plaintext buffer
}
