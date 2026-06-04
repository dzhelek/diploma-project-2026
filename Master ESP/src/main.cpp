#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "uart_protocol.h"
#include "algorithm_interface.h"

// put function declarations here:
int myFunction(int, int);
void setup_wifi();
void reconnect();

WiFiClient espClient;
PubSubClient client(espClient);

UartProtocol uartProtocol(Serial2);
RequestPacket  requestPkt;
ResponsePacket responsePkt;
UartStatus status;

IAlgorithm* algo;
AlgoResult result;
AlgoStatus status_algo;

static const UartAlgorithm ALGO_PREFERENCE[] = {
    ALGO_ASCON,
    ALGO_TINYJAMBU,
    ALGO_SCHWAEMM,
};
static const uint8_t ALGO_COUNT =
    sizeof(ALGO_PREFERENCE) / sizeof(ALGO_PREFERENCE[0]);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX, TX
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback([](char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
  });
  setup_wifi();

  status = uartProtocol.masterSendHi({ ALGO_PREFERENCE[0] });
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

  // generate pseudo-random 16-byte key and nonce for testing
  uint8_t demoKeyBytes[16];
  uint8_t demoNonceBytes[16];
  for (int i = 0; i < 16; i++) {
    demoKeyBytes[i] = random(256);
    demoNonceBytes[i] = random(256);
  }

  requestPkt.algorithm = ALGO_PREFERENCE[0];
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

  algo = AlgorithmFactory(requestPkt.algorithm);

  status_algo = algo->decrypt(
      responsePkt.data, responsePkt.dataSize,
      nullptr, 0, // No tag in this demo
      requestPkt.key,  requestPkt.keySize,
      requestPkt.nonce, requestPkt.nonceSize,
      nullptr, 0, // No associated data
      result
  );
  delete[] responsePkt.data; // Clean up dynamically allocated memory

  reconnect();

  if (status_algo != ALGO_OK) {
    String msg = "Decryption failed: status " + String(status_algo);
    client.publish("esp32/uart", msg.c_str());
  }
  else if (result.outputSize != requestPkt.dataSize ||
             memcmp(result.output, demoData, result.outputSize) != 0) {
    client.publish("esp32/uart", "Decryption succeeded but content is incorrect");
  } else {
    client.publish("esp32/uart", "Decryption succeeded and content is correct");
  }
}

void loop() {
  // if (!client.connected()) reconnect();
  // client.loop();

  // if (Serial2.available()) {
  //   String data = Serial2.readStringUntil('\n');
  //   Serial.println("Received from Serial2: " + data);
  //   client.publish("esp32/serial", data.c_str());
  // }

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP: " + WiFi.localIP().toString());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {  // Уникален Client ID
      Serial.println("connected");
      client.subscribe("esp32/output");   // subscription
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5s");
      delay(5000);
    }
  }
}