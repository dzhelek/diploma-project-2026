#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "algorithm_interface.h"
#include "master.h"

// put function declarations here:
void setup_wifi();
void reconnect();

WiFiClient espClient;
PubSubClient client(espClient);

RequestPacket  requestPkt;
ResponsePacket responsePkt;

IAlgorithm* algo;
AlgoStatus status_algo;


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

  request_slave(requestPkt, responsePkt);

  uint8_t* resultBuffer = new uint8_t[requestPkt.dataSize];
  size_t resultSize = 0;

  algo = AlgorithmFactory(requestPkt.algorithm);

  status_algo = algo->decrypt(
      responsePkt.data, responsePkt.dataSize,
      nullptr, 0, // No tag in this demo
      requestPkt.key,  requestPkt.keySize,
      requestPkt.nonce, requestPkt.nonceSize,
      nullptr, 0, // No associated data
      resultBuffer, &resultSize
  );
  delete[] responsePkt.data; // Clean up dynamically allocated memory

  reconnect();

  client.publish("esp32/uart", algo->name());
  if (status_algo != ALGO_OK) {
    String msg = "Decryption failed: status " + String(status_algo);
    client.publish("esp32/uart", msg.c_str());
  }
  else if (resultSize != requestPkt.dataSize ||
             memcmp(resultBuffer, requestPkt.data, resultSize) != 0) {
    client.publish("esp32/uart", "Decryption succeeded but content is incorrect");
    client.publish("esp32/uart", String(resultBuffer, resultSize).c_str());
    client.publish("esp32/uart", String(requestPkt.data, requestPkt.dataSize).c_str());
  } else {
    client.publish("esp32/uart", "Decryption succeeded and content is correct");
    client.publish("esp32/uart", String(responsePkt.timeMs).c_str());
  }

  delete[] resultBuffer; // Clean up dynamically allocated memory
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