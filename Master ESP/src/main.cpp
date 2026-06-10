#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

#include "secrets.h"
#include "algorithm_interface.h"
// #include "master.h"
#include "slave_node.h"

// put function declarations here:
void setup_wifi();
void reconnect();
void benchmark();
void printResult(const BenchmarkResult&);
void publishResult(const BenchmarkResult& r);

WiFiClient espClient;
PubSubClient client(espClient);

RequestPacket  requestPkt;
ResponsePacket responsePkt;

IAlgorithm* algo;
AlgoStatus status_algo;

const UartAlgorithm ALGO_PREFERENCE[] = {
    ALGO_ASCON,
    ALGO_TINYJAMBU,
    ALGO_SCHWAEMM,
};
const uint8_t ALGO_COUNT =
    sizeof(ALGO_PREFERENCE) / sizeof(ALGO_PREFERENCE[0]);

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX, TX
  Wire.begin();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback([](char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
  });

  setup_wifi();

  benchmark();
  /*

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
  */
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

void benchmark() {
  BenchmarkResult result;

  for (uint8_t n = 0; n < ALL_NODE_COUNT; ++n) {
        SlaveNode* node = ALL_NODES[n];

        Serial.print("━━━ Node: "); Serial.print(node->name());
        Serial.println(" ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

        for (uint8_t a = 0; a < ALGO_COUNT; ++a) {
            IAlgorithm* algo = AlgorithmFactory(ALGO_PREFERENCE[a]);
            if (!algo) {
                Serial.print("  [SKIP] Algorithm 0x");
                Serial.print(ALGO_PREFERENCE[a], HEX);
                Serial.println(" not implemented on master.");
                continue;
            }

            Serial.print("  ── Algorithm: "); Serial.println(algo->name());

            // Track whether this algorithm is supported by this node at all.
            // Set to false on first NACK; remaining sizes are skipped.
            bool algoSupported = true;

            for (uint8_t d = 0; d < BENCH_DATA_SIZE_COUNT; ++d) {
                uint16_t dataSize = BENCH_DATA_SIZES[d];

                if (!algoSupported) {
                    Serial.print("     [SKIP] ");
                    Serial.print(dataSize);
                    Serial.println(" B — algorithm not supported by this node.");
                    continue;
                }

                Serial.print("     Running ");
                Serial.print(dataSize);
                Serial.print(" B ... ");

                UartStatus us = node->runBenchmark(algo, dataSize, result);

                if (us == UART_ERR_NACK) {
                    // Slave rejected the algorithm — skip all remaining sizes
                    algoSupported = false;
                    Serial.println("NACK (algorithm not supported).");
                    continue;
                }

                if (us != UART_OK) {
                    // Comms or parameter error on this size; log and continue
                    Serial.print("FAILED (");
                    Serial.print(String(us));
                    Serial.println(").");
                    printResult(result);
                    publishResult(result);
                    continue;
                }

                Serial.println("OK.");
                printResult(result);
                publishResult(result);

                // Small inter-run delay to let the slave reset its state
                delay(50);
            }

            Serial.println();
        }

        Serial.println();
    }
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

/**
 * Print one BenchmarkResult as a human-readable block to USB Serial.
 */
void printResult(const BenchmarkResult& r)
{
    Serial.println("┌─────────────────────────────────────────");
    Serial.print  ("│ Slave      : "); Serial.println(r.slaveName);
    Serial.print  ("│ Algorithm  : "); Serial.println(r.algorithmName);
    Serial.print  ("│ Data size  : "); Serial.print(r.dataSize); Serial.println(" B");
    Serial.print  ("│ UART status: "); Serial.println(String(r.uartStatus));
    Serial.print  ("│ Algo status: "); Serial.println(String(r.algoStatus));
    Serial.print  ("│ Decrypt OK : "); Serial.println(r.decryptOk ? "YES" : "NO");

    if (r.uartStatus == UART_OK) {
        Serial.print("│ Slave time : "); Serial.print(r.slaveTimeMs);  Serial.println(" ms");
        Serial.println("│ ── Power ──────────────────────────────");
        Serial.print("│ Idle power : "); Serial.print(r.idlePowerMW,  2); Serial.println(" mW");
        Serial.print("│ Avg  power : "); Serial.print(r.avgPowerMW,   2); Serial.println(" mW");
        Serial.print("│ Peak power : "); Serial.print(r.peakPowerMW,  2); Serial.println(" mW");
        Serial.print("│ Δ power    : "); Serial.print(r.deltaPowerMW, 2); Serial.println(" mW");
        Serial.print("│ Energy     : "); Serial.print(r.energyMJ,     4); Serial.println(" mJ");
    }
    Serial.println("└─────────────────────────────────────────\n");
}

static String sanitizeTopicToken(const char* token)
{
    String s = token ? String(token) : String();
    s.replace(" ", "_");
    s.replace("/", "_");
    s.replace("+", "_");
    s.replace("#", "_");
    s.replace("\"", "");
    return s;
}

void publishResult(const BenchmarkResult& r)
{
    reconnect();

    String topic = String("crypto_bench/") + sanitizeTopicToken(r.slaveName) + "/" +
                   sanitizeTopicToken(r.algorithmName) + "/" +
                   String(r.dataSize) + "/result";

    String payload = "{";
    payload += "\"slaveName\":\"" + String(r.slaveName) + "\",";
    payload += "\"algorithmName\":\"" + String(r.algorithmName) + "\",";
    payload += "\"dataSize\":" + String(r.dataSize) + ",";
    payload += "\"decryptOk\":" + String(r.decryptOk ? "true" : "false") + ",";
    payload += "\"slaveTimeMs\":" + String(r.slaveTimeMs) + ",";
    payload += "\"idlePowerMW\":" + String(r.idlePowerMW, 2) + ",";
    payload += "\"avgPowerMW\":" + String(r.avgPowerMW, 2) + ",";
    payload += "\"peakPowerMW\":" + String(r.peakPowerMW, 2) + ",";
    payload += "\"deltaPowerMW\":" + String(r.deltaPowerMW, 2) + ",";
    payload += "\"energyMJ\":" + String(r.energyMJ, 4) + ",";
    payload += "\"uartStatus\":" + String((int)r.uartStatus) + ",";
    payload += "\"algoStatus\":" + String((int)r.algoStatus);
    payload += "}";

    client.publish(topic.c_str(), payload.c_str());
}