#include "slave_node.h"
#include <Wire.h>

SlaveNode NODE_ESPRESSIF32  ("espressif32",   16,   17,  0x40);
SlaveNode NODE_ESPRESSIF8266("espressif8266", 33,   32,  0x41);
SlaveNode NODE_RASPBERRYPI  ("raspberrypi",   22,   23,  0x44);
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
    for (int i = 0; i < dataSize; ++i) { _plaintextBuf[i] = random(256); }
}

void SlaveNode::activateUart()
{
    // Remap Serial2 to this node's GPIO pins then (re)start it
    // Serial2.end();
    Serial2.begin(9600, SERIAL_8N1, _rxPin, _txPin);
    while(!Serial2);
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
    result.slaveTimeMs    = 0;
    result.idlePowerMW    = 0.0f;
    result.avgPowerMW     = 0.0f;
    result.peakPowerMW    = 0.0f;
    result.deltaPowerMW   = 0.0f;
    result.energyMJ       = 0.0f;
    result.uartStatus     = UART_OK;
    result.algoStatus     = ALGO_OK;

    activateUart();

    // ── Step 1: Hi handshake ──────────────────────────────────────────────
    HiPacket hiPkt;
    hiPkt.algorithm = algo->id();

    Serial.print("Sending Hi...");
    UartStatus us = _uart.masterSendHi(hiPkt);
    result.uartStatus = us;

    if (us != UART_OK) {
        deactivateUart();
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
        deactivateUart();
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

    if (_sensor.begin()) {
        _sensor.startMeasurement();
        do {
            _sensor.update();
        } while(!Serial2.available());
    }

    Serial.print("Receiving Response...");
    ResponsePacket respPkt;
    us = _uart.masterReceiveResponse(respPkt);
    result.uartStatus = us;

    INAWindow window = _sensor.stopMeasurement();
    result.slaveTimeMs  = respPkt.timeMs;
    result.avgPowerMW   = window.avgPowerMW;
    result.peakPowerMW  = window.peakPowerMW;
    result.deltaPowerMW = window.deltaPowerMW;
    result.energyMJ     = window.energyMJ;
    result.idlePowerMW  = window.idlePowerMW;

    if (us != UART_OK) {
        deactivateUart();
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
        deactivateUart();
        return BENCH_DECRYPT_ERR;
    }

    // const uint8_t* ciphertext = respPkt.data;
    // const uint8_t* tag        = respPkt.data + dataSize;

    uint8_t* decOutputBuf = new uint8_t[dataSize];
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

    deactivateUart();
    delete[] _plaintextBuf;
    delete[] decOutputBuf;
    delete[] respPkt.data;
    return BENCH_OK;
}
