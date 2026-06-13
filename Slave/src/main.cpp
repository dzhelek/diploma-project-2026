#include <Arduino.h>

#include "slave.h"
#include "algorithm_interface.h"
#include "slave_debug.h"

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
   Serial.begin(115200);   // USB CDC console for debug (separate from the Serial1 link)
   while (!Serial && millis() < 3000) { }        // let the USB CDC port enumerate, but never hang on it
   // UART0 TX/RX must be a hardware-valid pair: GP0/1, GP12/13, GP16/17, or GP28/29.
   // GP14/GP15 are UART0 CTS/RTS (NOT TX/RX) — the core silently keeps the defaults, so nothing reaches them.
   Serial1.setTX(12);
   Serial1.setRX(13);
   Serial1.setFIFOSize(UART_RX_BUFFER_SIZE);      // Philhower RX FIFO defaults to 32 B — too small; must precede begin()
   Serial1.begin(UART_BAUD);
#endif
#ifdef SLAVE_ARDUINO_NANO
   Serial.begin(UART_BAUD);
#endif

  SLAVE_DBGLN("Setup complete");
}

void loop() {
   while(!LINK_SERIAL.available());

  while(!wait_for_master(requestPkt)) delay(100);

  algo = AlgorithmFactory(requestPkt.algorithm);
  if (!algo) {
    SLAVE_DBGLN("Unsupported algorithm in request, ignoring");
    delete[] requestPkt.data;
    requestPkt.data = nullptr;
    return; // restart loop()
  }

  startMs = micros();

  size_t allocSize = requestPkt.dataSize + algo->tagSize();
  size_t responsePktDataSize = allocSize;
  responsePkt.data = new uint8_t[allocSize]; // Allocate buffer for ciphertext + tag
  if (!responsePkt.data) {
    SLAVE_DBGLN("Out of memory allocating response buffer");
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
    SLAVE_DBGLN("Encrypt failed, status: " + String(status_algo));
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
