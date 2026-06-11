#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "uart_protocol.h"
#include "algorithm_interface.h"
#include "ina219_sensor.h"

enum BenchmarkStatus : int8_t {
    BENCH_OK = 0,
    BENCH_HI_NACK = -10,
    BENCH_HI_TIMEOUT = -11,
    BENCH_HI_ERR = -12,
    BENCH_REQUEST_NACK = -20,
    BENCH_REQUEST_TIMEOUT = -21,
    BENCH_REQUEST_ERR = -22,
    BENCH_RESPONSE_TIMEOUT = -30,
    BENCH_DECRYPT_ERR = -40,
};

static const uint16_t BENCH_DATA_SIZES[] = {
    64,
    256,
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
};
static const uint8_t BENCH_DATA_SIZE_COUNT =
    sizeof(BENCH_DATA_SIZES) / sizeof(BENCH_DATA_SIZES[0]);

struct BenchmarkResult {
    const char*   slaveName;       // Points to the SlaveNode::name() string
    const char*   algorithmName;   // Points to IAlgorithm::name() string

    uint16_t      dataSize;        // Plaintext size used (bytes)
    bool          decryptOk;       // true if decrypt + plaintext match passed

    uint32_t      slaveTimeMs;     // Processing time reported by slave (ms)

    float         idlePowerMW;     // Baseline power before request (mW)
    float         avgPowerMW;      // Average power during slave processing (mW)
    float         peakPowerMW;     // Peak power during slave processing (mW)
    float         deltaPowerMW;    // peakPower − idlePower (mW)
    float         energyMJ;        // Total energy consumed during window (mJ)

    // ── Status ────────────────────────────────────────────────────────────
    UartStatus    uartStatus;      // Last UART-layer status code
    AlgoStatus    algoStatus;      // Last algorithm-layer status code
};

class SlaveNode {
public:
    SlaveNode(const char* name,
              uint8_t     rxPin,
              uint8_t     txPin,
              uint8_t     inaAddress)
        : _name(name)
        , _rxPin(rxPin)
        , _txPin(txPin)
        , _sensor(inaAddress)
        , _uart(Serial2)
    {}

    const char* name()     const { return _name; }
    uint8_t     rxPin()    const { return _rxPin; }
    uint8_t     txPin()    const { return _txPin; }

    void activateUart();

    void deactivateUart();

    /**
     * Run a complete benchmark session for one algorithm × one data size:
     *
     *   1. activateUart()
     *   2. Hi handshake (checks algorithm support)
     *   3. Measure idle power
     *   4. Send request 
     *   5. Wait for slave ACK
     *   6. Wait for response
     *   7. Receive ACK from slave
     *   8. Decrypt response and compare against sent plaintext
     *   9. deactivateUart()
     *  10. Populate and return BenchmarkResult
     */
    BenchmarkStatus runBenchmark(IAlgorithm*       algo,
                            uint16_t          dataSize,
                            BenchmarkResult&  result);

private:
    const char*  _name;
    uint8_t      _rxPin;
    uint8_t      _txPin;
    INA219Sensor _sensor;
    UartProtocol _uart;

    uint8_t* _plaintextBuf;
    uint8_t _keyBuf      [UART_MAX_KEY_SIZE];
    uint8_t _nonceBuf    [UART_MAX_NONCE_SIZE];

    void prepareBuffers(IAlgorithm* algo, size_t dataSize);
};

extern SlaveNode NODE_ESPRESSIF32;
extern SlaveNode NODE_ESPRESSIF8266;
extern SlaveNode NODE_RASPBERRYPI;
extern SlaveNode NODE_ATMELAVR;

/** Flat array of all nodes — iterate to run the full benchmark sweep. */
extern SlaveNode* const ALL_NODES[];
extern const uint8_t    ALL_NODE_COUNT;
