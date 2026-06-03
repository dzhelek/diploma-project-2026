#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "secrets.h"
#include "uart_protocol.h"

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
  const char* demoKey  = "secretkey123";

  requestPkt.algorithm = ALGO_PREFERENCE[0];
  requestPkt.dataSize  = (uint16_t)strlen(demoData);
  requestPkt.keySize   = (uint16_t)strlen(demoKey);
  memcpy(requestPkt.data, demoData, requestPkt.dataSize);
  memcpy(requestPkt.key,  demoKey,  requestPkt.keySize);

  status = uartProtocol.masterSendRequest(requestPkt);
  if (status != UART_OK) {
    Serial.println("Failed to send request to slave");
    return;
  }
  else {
    Serial.println("Request sent to slave");
  }

  status = uartProtocol.masterReceiveResponse(responsePkt);

  reconnect();
  if (status != UART_OK) {
    client.publish("esp32/uart", "Failed to receive response from slave");
    Serial.println("Failed to receive response from slave");
  }
  else {
    client.publish("esp32/uart", "Received response from slave");
    client.publish("esp32/uart/time", String(responsePkt.timeMs).c_str());
    Serial.print("Received response from slave: ");
    Serial.write(responsePkt.data, responsePkt.dataSize);
    Serial.println();
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