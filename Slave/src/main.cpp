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
#ifdef SLAVE_ESP32
   Serial.begin(115200);
   Serial2.setRxBufferSize(UART_RX_BUFFER_SIZE); // before begin(): hold large payloads
   Serial2.begin(UART_BAUD, SERIAL_8N1, 16, 17); // RX, TX
   while(!Serial);
#endif
#ifdef SLAVE_ESP_12
   Serial.begin(9600);
#endif
#ifdef SLAVE_RASPBERRY_PI_PICO
   Serial1.setTX(14);
   Serial1.setRX(15);
   Serial1.begin(9600);
#endif
#ifdef SLAVE_ARDUINO_NANO
   Serial.begin(9600);
#endif

  Serial.println("Setup complete");
}

void loop() {
   while(!Serial2.available());

  while(!wait_for_master(requestPkt)) delay(100);

  algo = AlgorithmFactory(requestPkt.algorithm);
  if (!algo) {
    Serial.println("Unsupported algorithm in request, ignoring");
    delete[] requestPkt.data;
    requestPkt.data = nullptr;
    return; // restart loop()
  }

  startMs = micros();

  size_t allocSize = requestPkt.dataSize + algo->tagSize();
  size_t responsePktDataSize = allocSize;
  responsePkt.data = new uint8_t[allocSize]; // Allocate buffer for ciphertext + tag
  if (!responsePkt.data) {
    Serial.println("Out of memory allocating response buffer");
    delete[] requestPkt.data;
    requestPkt.data = nullptr;
    return; // restart loop()
  }
  status_algo = algo->encrypt(
      requestPkt.data, requestPkt.dataSize,
      requestPkt.key,  requestPkt.keySize,
      requestPkt.nonce, requestPkt.nonceSize,
      nullptr, 0, // No associated data
      responsePkt.data, &responsePktDataSize
  );

  elapsed = micros() - startMs;   // unsigned: correct even across a micros() wrap (~71 min)

  if (status_algo != ALGO_OK) {
    Serial.println("Encrypt failed, status: " + String(status_algo));
  }

  // Never transmit more than we allocated, even if encrypt misreported its output
  // length — slaveSendResponse reads dataSize bytes from responsePkt.data.
  if (responsePktDataSize > allocSize) responsePktDataSize = allocSize;

  responsePkt.dataSize = responsePktDataSize;
  responsePkt.timeUs = elapsed;

  respond_to_master(responsePkt);

  delete[] responsePkt.data; // Clean up ciphertext buffer
  delete[] requestPkt.data;  // Clean up plaintext buffer
}
