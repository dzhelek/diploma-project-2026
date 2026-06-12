#include "ina219_sensor.h"

#include <Adafruit_INA219.h>

INA219Sensor::INA219Sensor(uint8_t i2cAddress)
    : _address(i2cAddress)
    , _idlePowerMW(0.0f)
    , _windowStartMs(0)
    , _accumPowerMW(0.0f)
    , _peakPowerMW(0.0f)
    , _sampleCount(0)
    , _lastSampleMs(0)
    , _sensor(nullptr)
{}

bool INA219Sensor::begin()
{
    // Allocate the driver once and reuse it across measurement windows.
    if (!_sensor) {
        _sensor = new Adafruit_INA219(_address);
        if (!_sensor) return false;
    }

    // begin() returns true if the sensor responds on the bus
    if (!_sensor->begin()) return false;

    return true;
}

float INA219Sensor::readPower()
{
    float voltageV  = _sensor->getBusVoltage_V();
    float currentmA = _sensor->getCurrent_mA();
    return voltageV * currentmA; // mW  (V × mA = mW)
}

void INA219Sensor::update()
{
    float powerMW = readPower();
    _accumPowerMW += powerMW;
    if (powerMW > _peakPowerMW) _peakPowerMW = powerMW;
    ++_sampleCount;
    _lastSampleMs = millis();   // window ends at the last sample, not at stop time
}

void INA219Sensor::startMeasurement()
{
    // Reset window accumulators so each measurement is independent of the last.
    _idlePowerMW   = readPower();
    _accumPowerMW  = 0.0f;
    _peakPowerMW   = 0.0f;
    _sampleCount   = 0;
    _windowStartMs = millis();
    _lastSampleMs  = _windowStartMs;
}

INAWindow INA219Sensor::stopMeasurement()
{
    INAWindow w;
    // durationMs spans only the sampling window (start -> last sample), so it
    // matches the samples that fed avgPowerMW; the response-transfer tail is excluded.
    w.durationMs = _lastSampleMs - _windowStartMs;
    if (w.durationMs == 0) w.durationMs = 1;   // avoid a degenerate 0 ms window (energy would read 0)
    w.avgPowerMW   = (_sampleCount > 0)
                     ? (_accumPowerMW / _sampleCount)
                     : 0.0f;
    w.peakPowerMW  = _peakPowerMW;
    w.idlePowerMW = _idlePowerMW;
    w.deltaPowerMW = _peakPowerMW - _idlePowerMW;

    // Energy (mJ) = avgPower (mW) × time (s)
    w.energyMJ     = w.avgPowerMW * (float)w.durationMs / 1000.0f;

    return w;
}
