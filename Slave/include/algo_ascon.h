#pragma once

#include "algorithm_interface.h"

class AsconAlgorithm : public IAlgorithm {
public:
    const char* name()      const override { return "Ascon-128"; }
    size_t      keySize()   const override { return 16; }
    size_t      nonceSize() const override { return 16; }
    size_t      tagSize()   const override { return 16; }

    AlgoStatus encrypt(
        const uint8_t* plaintext,   size_t plaintextSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        uint8_t* result,            size_t* resultSize
    ) override;
};
