#include <Arduino.h>

#include "crypto_aead.h"
// #include "yoan.h"

#include "uart_protocol.h"

// put function declarations here:
int myFunction(int, int);
bool isAlgorithmSupported(UartAlgorithm algo);
void setup_wifi();
uint16_t runAlgorithm(const RequestPacket& request, ResponsePacket& response);

UartProtocol uartProtocol(Serial2);
RequestPacket  requestPkt;
ResponsePacket responsePkt;
UartStatus status;
HiPacket hiPkt;

UartAlgorithm SUPPORTED_ALGOS[] = {
    ALGO_ASCON,
};
const uint8_t SUPPORTED_ALGO_COUNT =
    sizeof(SUPPORTED_ALGOS) / sizeof(SUPPORTED_ALGOS[0]);


// Ascon-AEAD128 example
  unsigned char n[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                         11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                         22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  unsigned char k[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                         11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                         22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  unsigned char a[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                         11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                         22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  unsigned char m[32] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                         11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                         22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  unsigned char c[32], h[32], t[32];
  unsigned long long alen = 16;
  unsigned long long mlen = 16;
  unsigned long long clen;
  int result = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX, TX
  // put your setup code here, to run once:
  // int result = myFunction(2, 3);

  //   printf("input:\n");
  // result |= crypto_aead_encrypt(c, &clen, m, mlen, a, alen, (unsigned char *)0, n, k);
  // printf("encrypt:\n");
  // result |= crypto_aead_decrypt(m, &mlen, (unsigned char *)0, c, clen, a, alen, n, k);
  // printf("decrypt:\n");
  // yoan();

  Serial.println("Setup complete");

}

void loop() {
  // put your main code here, to run repeatedly:

  // static unsigned long lastMsg = 0;
  // if (millis() - lastMsg > 5000) {
  //   lastMsg = millis();
  //   const char* msg = "hello";
  //   Serial.println("Publishing message: " + String(msg));
  //   Serial2.println("Hello from ESP32 Slave Serial2!");
  // }
  delay(1000);

  Serial.println("Waiting for Hi");
  status = uartProtocol.slaveReceiveHi(hiPkt, SUPPORTED_ALGOS, SUPPORTED_ALGO_COUNT);
  if (status == UART_ERR_NACK) {
    Serial.println("Master requested unsupported algorithm");
    return;
  } else if (status != UART_OK) {
    Serial.println("Failed to receive Hi from master");
    return;
  } else {
    Serial.print("Received Hi from master, algorithm: ");
    Serial.println(hiPkt.algorithm);
  }

  status = uartProtocol.slaveReceiveRequest(requestPkt);
  if (status != UART_OK) {
    Serial.println("Failed to receive request from master");
    return;
  } else {
    Serial.println("Received request from master");
  }

  uint32_t startMs = millis();
  uint16_t outLen  = runAlgorithm(requestPkt, responsePkt);
  uint32_t elapsed = millis() - startMs;

  responsePkt.timeMs = elapsed > 0xFFFF ? 0xFFFF : (uint16_t)elapsed;
  responsePkt.dataSize = outLen;

  status = uartProtocol.slaveSendResponse(responsePkt);
  if (status != UART_OK) {
    Serial.println("Failed to send response to master");
    return;
  } else {
    Serial.println("Sent response to master");
  }

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}

/** Return true if this slave supports @p algo. */
bool isAlgorithmSupported(UartAlgorithm algo)
{
    for (uint8_t i = 0; i < SUPPORTED_ALGO_COUNT; ++i) {
        if (SUPPORTED_ALGOS[i] == algo) return true;
    }
    return false;
}

/**
 * Stub: run the requested algorithm on the data + key from @p req and write
 * the result into @p resp.
 *
 * Replace the body of each case with a real library call.
 * Returns the number of output bytes written into resp.data, or 0 on error.
 */
uint16_t runAlgorithm(const RequestPacket& req, ResponsePacket& resp)
{
    switch (req.algorithm) {

        case ALGO_ASCON: {
            // TODO: call Ascon encrypt/decrypt here.
            // Example (pseudocode):
            //   ascon_encrypt(resp.data, req.data, req.dataSize,
            //                 req.key,  req.keySize);
            //   return outputLen;
            //
            // Stub: echo the input back
            memcpy(resp.data, req.data, req.dataSize);
            return req.dataSize;
        }
        default:
            return 0;
    }
}