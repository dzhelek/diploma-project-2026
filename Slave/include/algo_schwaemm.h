#pragma once

#include "algorithm_interface.h"

/**
 * SCHWAEMM256-128 AEAD implementation.
 *
 * Parameters (SCHWAEMM256-128 from the Sparkle suite):
 *   Key   : 16 bytes
 *   Nonce : 32 bytes
 *   Tag   : 16 bytes
 *   AD    : variable length (may be empty)
 *
 * Replace the stub bodies in schwaemm_algo.cpp with a real SCHWAEMM
 * library call, e.g. from https://github.com/sparkle-aead/sparkle
 */
class SCHWAEMMAlgorithm : public IAlgorithm {
public:
    // ── IAlgorithm interface ──────────────────────────────────────────────

    const char* name()      const override { return "SCHWAEMM192-192"; }
    size_t      keySize()   const override { return 24; }
    size_t      nonceSize() const override { return 24; }
    size_t      tagSize()   const override { return 24; }

    AlgoStatus encrypt(
        const uint8_t* plaintext,  size_t plaintextSize,
        const uint8_t* key,        size_t keySize,
        const uint8_t* nonce,      size_t nonceSize,
        const uint8_t* ad,         size_t adSize,
        AlgoResult&    result
    ) override;

private:
    uint8_t _outputBuf[ALGO_MAX_CIPHERTEXT_SIZE];
    uint64_t  _outputSize;
    uint8_t _tagBuf   [ALGO_MAX_TAG_SIZE];
};
