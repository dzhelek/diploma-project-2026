#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Adafruit_INA219.h"

struct INAWindow {
    float avgPowerMW;    // Average power over the window (mW)
    float peakPowerMW;   // Peak instantaneous power seen (mW)
    float deltaPowerMW;  // peakPowerMW − idlePowerMW at window start (mW)
    float idlePowerMW;
    float energyMJ;      // Total energy consumed in the window (mJ)
    uint32_t durationMs;
};

class INA219Sensor {
public:
    explicit INA219Sensor(uint8_t i2cAddress);
    bool begin();
    float readPower();
    void update();
    void startMeasurement();

    INAWindow stopMeasurement();

    // Sampling interval used during a measurement window (ms)
    static constexpr uint32_t SAMPLE_INTERVAL_MS = 10;

private:
    uint8_t  _address;

    float    _idlePowerMW;
    uint32_t _windowStartMs;
    float    _accumPowerMW;   // running sum for average
    float    _peakPowerMW;
    uint32_t _sampleCount;
    uint32_t _lastSampleMs;

    Adafruit_INA219* _sensor;

    // Internal: read raw registers without updating window state
    // INAReading _readRaw();
};
