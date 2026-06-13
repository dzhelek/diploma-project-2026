#include "slave_node.h"
#include <Wire.h>

SlaveNode NODE_ESPRESSIF32  ("espressif32",   16,   17,  0x40);
SlaveNode NODE_ESPRESSIF8266("espressif8266", 33,   32,  0x41);
SlaveNode NODE_RASPBERRYPI  ("raspberrypi",   27,   26,  0x44);
SlaveNode NODE_ATMELAVR     ("atmelavr",      19,   18,  0x45);

SlaveNode* const ALL_NODES[] = {
    &NODE_ESPRESSIF32,
    &NODE_ESPRESSIF8266,
    &NODE_RASPBERRYPI,
    &NODE_ATMELAVR,
};
const uint8_t ALL_NODE_COUNT = sizeof(ALL_NODES) / sizeof(ALL_NODES[0]);

void SlaveNode::prepareBuffers(IAlgorithm* algo, size_t dataSize)
{
    int keySize = algo->keySize();
    int nonceSize = algo->nonceSize();
    for (int i = 0; i < keySize; ++i) { _keyBuf[i] = random(256); }
    for (int i = 0; i < nonceSize; ++i) { _nonceBuf[i] = random(256); }
    for (size_t i = 0; i < dataSize; ++i) { _plaintextBuf[i] = random(256); }
}

void SlaveNode::activateUart()
{
    // (Re)map Serial2 to this node's GPIO pins and start the link. Call this ONCE
    // per node (not per run): re-initialising mid-conversation glitches the line and
    // can corrupt the final ACK / next Hi, desyncing the two state machines.
    //
    // end() first so setRxBufferSize() takes effect (it is ignored once the driver is
    // installed). This end() is at a node boundary, not mid-exchange, so it is safe.
    // A larger RX buffer + faster baud keep large payloads from overflowing the buffer
    // (WiFi/MQTT can preempt the read loop) or exceeding the per-read timeout.
    Serial2.end();
    Serial2.setRxBufferSize(UART_RX_BUFFER_SIZE);
    Serial2.begin(UART_BAUD, SERIAL_8N1, _rxPin, _txPin);
    delay(50);                                    // let the UART settle after (re)config
    while (Serial2.available()) Serial2.read();   // discard any line-glitch bytes from begin()
}

void SlaveNode::deactivateUart()
{
    Serial2.end();
}

// ─────────────────────────────────────────────────────────────────────────────
//  runBenchmark
// ─────────────────────────────────────────────────────────────────────────────

BenchmarkStatus SlaveNode::runBenchmark(IAlgorithm*      algo,
                                   uint16_t         dataSize,
                                   BenchmarkResult& result)
{
    // ── Initialise result fields ──────────────────────────────────────────
    result.slaveName      = _name;
    result.algorithmName  = algo->name();
    result.dataSize       = dataSize;
    result.decryptOk      = false;
    result.slaveTimeUs    = 0;
    result.idlePowerMW    = 0.0f;
    result.avgPowerMW     = 0.0f;
    result.peakPowerMW    = 0.0f;
    result.deltaPowerMW   = 0.0f;
    result.energyMJ       = 0.0f;
    result.uartStatus     = UART_OK;
    result.algoStatus     = ALGO_OK;

    // The UART link is opened once per node by the caller (activateUart) and kept
    // up across every run, so it is intentionally NOT (re)started here.

    // ── Step 1: Hi handshake ──────────────────────────────────────────────
    HiPacket hiPkt;
    hiPkt.algorithm = algo->id();

    Serial.print("Sending Hi...");
    UartStatus us = _uart.masterSendHi(hiPkt);
    result.uartStatus = us;

    if (us != UART_OK) {
        switch(us) {
            case UART_ERR_NACK:
                return BENCH_HI_NACK;
            case UART_ERR_TIMEOUT:
                return BENCH_HI_TIMEOUT;
            default:
                return BENCH_HI_ERR;
        }
    }
    Serial.println("OK!");

    _plaintextBuf = new uint8_t[dataSize];
    if (!_plaintextBuf) {
        result.uartStatus = UART_ERR_OVERFLOW;
        return BENCH_REQUEST_ERR;
    }
    prepareBuffers(algo, dataSize);

    RequestPacket reqPkt;
    reqPkt.algorithm = algo->id();
    reqPkt.dataSize  = dataSize;
    reqPkt.keySize   = (uint16_t)algo->keySize();
    reqPkt.nonceSize = algo->nonceSize();
    reqPkt.data = _plaintextBuf;
    memcpy(reqPkt.key, _keyBuf, algo->keySize());
    memcpy(reqPkt.nonce, _nonceBuf, algo->nonceSize());

    Serial.print("Sending Request...");
    us = _uart.masterSendRequest(reqPkt);
    result.uartStatus = us;

    if (us != UART_OK) {
        delete[] _plaintextBuf;
        _plaintextBuf = nullptr;
         switch(us) {
            case UART_ERR_NACK:
                return BENCH_REQUEST_NACK;
            case UART_ERR_TIMEOUT:
                return BENCH_REQUEST_TIMEOUT;
            default:
                return BENCH_REQUEST_ERR;
        }
    }
    Serial.println("OK!");

    bool sensorOk = _sensor.begin();
    if (sensorOk) {
        _sensor.startMeasurement();
        // Sample until the slave starts replying, but bound the wait so a crashed
        // slave can't hang the master here (masterReceiveResponse has its own timeout).
        uint32_t sampleDeadline = millis() + (uint32_t)UART_TIMEOUT_MS * UART_MAX_RETRIES;
        do {
            _sensor.update();
        } while (!Serial2.available() && millis() < sampleDeadline);
    }

    Serial.print("Receiving Response...");
    ResponsePacket respPkt = {};   // zero-init so timeMs/dataSize aren't read uninitialised on timeout
    us = _uart.masterReceiveResponse(respPkt);
    result.uartStatus = us;

    result.slaveTimeUs = respPkt.timeUs;
    if (sensorOk) {
        // Only trust the window if the sensor was actually sampling this run.
        INAWindow window = _sensor.stopMeasurement();
        result.avgPowerMW   = window.avgPowerMW;
        result.peakPowerMW  = window.peakPowerMW;
        result.deltaPowerMW = window.deltaPowerMW;
        result.energyMJ     = window.energyMJ;
        result.idlePowerMW  = window.idlePowerMW;
    }

    if (us != UART_OK) {
        delete[] _plaintextBuf;
        _plaintextBuf = nullptr;
        delete[] respPkt.data;   // nullptr after a timeout; delete[] nullptr is safe
        return BENCH_RESPONSE_TIMEOUT;
    }
    Serial.println("OK!");


    // ── Step 8: Decrypt response and verify ───────────────────────────────
    //
    // Response data layout (packed by slave): [ciphertext | tag]
    // Tag size is algo->tagSize(); ciphertext size = dataSize.
    //
    Serial.print("Decrypting...");
    size_t expectedTotal = dataSize + algo->tagSize();

    if (respPkt.dataSize != (uint16_t)expectedTotal) {
        // Unexpected payload length — treat as protocol error
        result.uartStatus = UART_ERR_UNKNOWN;
        delete[] _plaintextBuf;
        _plaintextBuf = nullptr;
        delete[] respPkt.data;
        return BENCH_DECRYPT_ERR;
    }

    // const uint8_t* ciphertext = respPkt.data;
    // const uint8_t* tag        = respPkt.data + dataSize;

    uint8_t* decOutputBuf = new uint8_t[dataSize];
    if (!decOutputBuf) {
        delete[] _plaintextBuf;
        _plaintextBuf = nullptr;
        delete[] respPkt.data;
        result.algoStatus = ALGO_ERR_OVERFLOW;
        return BENCH_DECRYPT_ERR;
    }
    size_t decOutputLen = 0;
    AlgoStatus as = algo->decrypt(
        respPkt.data,  respPkt.dataSize,
        nullptr,       0,
        _keyBuf,       algo->keySize(),
        _nonceBuf,     algo->nonceSize(),
        nullptr,       0,               // no associated data in benchmark
        decOutputBuf,  &decOutputLen
    );
    result.algoStatus = as;

    if (as == ALGO_OK) {
        // Compare decrypted output against original plaintext
        result.decryptOk =
            (memcmp(decOutputBuf, _plaintextBuf, dataSize) == 0);
        Serial.println("OK!");
    }

    delete[] _plaintextBuf;
    delete[] decOutputBuf;
    delete[] respPkt.data;
    return BENCH_OK;
}
