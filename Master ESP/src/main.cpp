#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

#include "wolfssl.h"
#include "wolfssl/ssl.h"
#include "WolfSSLClient.h"

#include "secrets.h"
#include "cert.h"
#include "algorithm_interface.h"
#include "slave_node.h"

#include <time.h>      // with the includes
void setup_time();     // with the prototypes (~line 16)

void setup_time() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // UTC
  Serial.print("Waiting for NTP time");
  time_t now = time(nullptr);
  int tries = 40;
  while (now < 1700000000 && --tries > 0) { delay(500); Serial.print("."); now = time(nullptr); }
  Serial.printf("\nUTC now: %s", asctime(gmtime(&now)));
}
// put function declarations here:
void setup_wifi();
void reconnect();
void benchmark();
void printResult(const BenchmarkResult&);
void publishResult(const BenchmarkResult& r);

WiFiClient espClient;
WolfSSLClient tls(espClient);
PubSubClient client(tls);

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
  Serial2.begin(UART_BAUD, SERIAL_8N1, 16, 17); // RX, TX (reconfigured per node in activateUart)
  Wire.begin();

  wolfSSL_Init();
  WOLFSSL_CTX* probe = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
  bool ok = wolfSSL_CTX_set_cipher_list(probe, "TLS13-ASCONAEAD128-ASCONHASH256") == WOLFSSL_SUCCESS;
  Serial.println(ok ? "ASCON SUITE: present" : "ASCON SUITE: MISSING from this build");
  wolfSSL_CTX_free(probe);

  // tls.setCACert(root_ca);
  tls.setInsecure();
  tls.setCipherList("TLS13-ASCONAEAD128-ASCONHASH256");
  // tls.setServerName(MQTT_SERVER);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setKeepAlive(60);      // default 15 s is too short while a long benchmark run blocks client.loop()
  client.setSocketTimeout(30);  // default 15 s — give the wolfSSL (ASCON) TLS handshake + CONNACK more room
  client.setBufferSize(512);    // default 256 B silently drops the ~290 B crypto_bench result payload
  client.setCallback([](char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
  });

  setup_wifi();
  setup_time();
  reconnect();
  
  client.publish("esp32/mqtt", "connection successful");

  benchmark();

  client.publish("esp32/mqtt", "ready");

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
    client.publish("esp32/uart", String(responsePkt.timeUs).c_str());
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
  client.publish("esp32/mqtt", "benchmarking");
  BenchmarkResult result;
  bool skip_slave = false;

  for (uint8_t n = 0; n < ALL_NODE_COUNT; ++n) {
    SlaveNode* node = ALL_NODES[n];

    Serial.print("━━━ Node: "); Serial.print(node->name());
    Serial.println(" ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    // Open the UART link once for this node and keep it up across all of its runs.
    node->activateUart();

    for (uint8_t a = 0; a < ALGO_COUNT; ++a) {
      if (skip_slave) {
        skip_slave = false;
        break;
      }
      IAlgorithm* algo = AlgorithmFactory(ALGO_PREFERENCE[a]);
      if (!algo) {
          Serial.print("  [SKIP] Algorithm 0x");
          Serial.print(ALGO_PREFERENCE[a], HEX);
          Serial.println(" not implemented on master.");
          continue;
      }

      Serial.print("  ── Algorithm: "); Serial.println(algo->name());

      for (uint8_t d = 0; d < BENCH_DATA_SIZE_COUNT; ++d) {
        uint16_t dataSize = BENCH_DATA_SIZES[d];

        Serial.print("     Running ");
        Serial.print(dataSize);
        Serial.print(" B ... ");

        BenchmarkStatus bs = node->runBenchmark(algo, dataSize, result);
        Serial.println("bench status: " + String(bs));

        if (bs == BENCH_HI_NACK) {
          // Slave rejected the algorithm — skip all remaining sizes
          Serial.println("NACK (algorithm not supported).");
          break; // skip algorithm
        }

        if (bs == BENCH_HI_TIMEOUT) {
          Serial.println("Timeout (cannot connect to slave).");
          skip_slave = true;
          break;
        }

        if (bs != BENCH_OK) {
          // Comms or parameter error on this size; log and continue
          Serial.print("FAILED (");
          Serial.print(String(bs));
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

    // Done with this node — close its link before moving to the next node's pins.
    node->deactivateUart();

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
    if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {  // Уникален Client ID
      Serial.println("connected");
      client.subscribe("esp32/output");   // subscription
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5s");
      Serial.printf("WolfSSLClient err = %d\n", tls.lastError());
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
        Serial.print("│ Slave time : "); Serial.print(r.slaveTimeUs);  Serial.println(" us");
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
  client.publish("esp32/mqtt", "publishing1");
  reconnect();
  client.publish("esp32/mqtt", "publishing2");

  String topic = String("crypto_bench/") + sanitizeTopicToken(r.slaveName) + "/" +
                  sanitizeTopicToken(r.algorithmName) + "/" +
                  String(r.dataSize) + "/result";

  String payload = "{";
  payload += "\"slaveName\":\"" + String(r.slaveName) + "\",";
  payload += "\"algorithmName\":\"" + String(r.algorithmName) + "\",";
  payload += "\"dataSize\":" + String(r.dataSize) + ",";
  payload += "\"decryptOk\":" + String(r.decryptOk ? "true" : "false") + ",";
  payload += "\"slaveTimeUs\":" + String(r.slaveTimeUs) + ",";
  payload += "\"idlePowerMW\":" + String(r.idlePowerMW, 2) + ",";
  payload += "\"avgPowerMW\":" + String(r.avgPowerMW, 2) + ",";
  payload += "\"peakPowerMW\":" + String(r.peakPowerMW, 2) + ",";
  payload += "\"deltaPowerMW\":" + String(r.deltaPowerMW, 2) + ",";
  payload += "\"energyMJ\":" + String(r.energyMJ, 4) + ",";
  payload += "\"uartStatus\":" + String((int)r.uartStatus) + ",";
  payload += "\"algoStatus\":" + String((int)r.algoStatus);
  payload += "}";

  client.publish("esp32/mqtt", "publishing3");
  client.publish(topic.c_str(), payload.c_str());
  client.publish("esp32/mqtt", "publishing4");
}