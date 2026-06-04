#pragma once

#include "algorithm_interface.h"

/**
 * TinyJAMBU-128 AEAD implementation.
 *
 * Parameters (standard TinyJAMBU-128):
 *   Key   : 16 bytes
 *   Nonce : 12 bytes
 *   Tag   :  8 bytes
 *   AD    : variable length (may be empty)
 *
 * Replace the stub bodies in tinyjambu_algo.cpp with a real TinyJAMBU
 * library call, e.g. from https://github.com/nicowillis/TinyJAMBU
 */
class TinyJAMBUAlgorithm : public IAlgorithm {
public:
    // ── IAlgorithm interface ──────────────────────────────────────────────

    const char* name()      const override { return "TinyJAMBU-128"; }
    size_t      keySize()   const override { return 16; }
    size_t      nonceSize() const override { return 12; }
    size_t      tagSize()   const override { return  8; }

    AlgoStatus decrypt(
        const uint8_t* ciphertext,  size_t ciphertextSize,
        const uint8_t* tag,         size_t tagSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        AlgoResult&    result
    ) override;

private:
    uint8_t _outputBuf[ALGO_MAX_CIPHERTEXT_SIZE];
    uint8_t _tagBuf   [ALGO_MAX_TAG_SIZE];
};
