#pragma once

#include "algorithm_interface.h"

class SCHWAEMMAlgorithm : public IAlgorithm {
public:
    const char* name()      const override { return "SCHWAEMM192-192"; }
    UartAlgorithm id()      const override { return ALGO_SCHWAEMM; }
    size_t      keySize()   const override { return 24; }
    size_t      nonceSize() const override { return 24; }
    size_t      tagSize()   const override { return 24; }

    AlgoStatus decrypt(
        const uint8_t* ciphertext,  size_t ciphertextSize,
        const uint8_t* tag,         size_t tagSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        uint8_t* result,            size_t* resultSize
    ) override;
};
