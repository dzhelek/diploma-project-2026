#include <Arduino.h>

#include "crypto_aead.h"
// #include "yoan.h"

// put function declarations here:
int myFunction(int, int);

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

}

void loop() {
  // put your main code here, to run repeatedly:

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();
    const char* msg = "hello";
    Serial.println("Publishing message: " + String(msg));
    Serial2.println("Hello from ESP32 Slave Serial2!");
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}