#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "uart_protocol.h" // for UartAlgorithm enum

enum AlgoStatus : int8_t {
    ALGO_OK               =  0,  // Success
    ALGO_ERR_INVALID_KEY  = -1,  // Key size not accepted by this algorithm
    ALGO_ERR_INVALID_NONCE= -2,  // Nonce size not accepted by this algorithm
    ALGO_ERR_OVERFLOW     = -3,  // Input exceeds internal buffer
    ALGO_ERR_AUTH_FAIL    = -4,  // Tag verification failed during decrypt
    ALGO_ERR_NOT_IMPL     = -5,  // Algorithm not implemented on this device
    ALGO_ERR_UNKNOWN      = -6,  // Unspecified internal error
};

class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;

    virtual const char* name() const = 0;
    virtual UartAlgorithm id() const = 0;

    virtual size_t keySize()   const = 0;
    virtual size_t nonceSize() const = 0;
    virtual size_t tagSize()   const = 0;

    virtual AlgoStatus decrypt(
        const uint8_t* ciphertext,  size_t ciphertextSize,
        const uint8_t* tag,         size_t tagSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        uint8_t* result,            size_t* resultSize
    ) = 0;

protected:
    static AlgoStatus checkSize(size_t actual, size_t expected,
                                AlgoStatus errCode)
    {
        if (expected != 0 && actual != expected) return errCode;
        return ALGO_OK;
    }
};

IAlgorithm* AlgorithmFactory(UartAlgorithm algo);
