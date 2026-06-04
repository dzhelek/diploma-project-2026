#pragma once

#include "algorithm_interface.h"

/**
 * Ascon-128 AEAD implementation.
 *
 * Parameters (standard Ascon-128):
 *   Key   : 16 bytes
 *   Nonce : 16 bytes
 *   Tag   : 16 bytes
 *   AD    : variable length (may be empty)
 *
 * Replace the stub bodies in ascon_algo.cpp with a real Ascon library call,
 * e.g. the reference implementation from https://github.com/ascon/ascon-c
 */
class AsconAlgorithm : public IAlgorithm {
public:
    // ── IAlgorithm interface ──────────────────────────────────────────────

    const char* name()      const override { return "Ascon-128"; }
    size_t      keySize()   const override { return 16; }
    size_t      nonceSize() const override { return 16; }
    size_t      tagSize()   const override { return 16; }

    AlgoStatus decrypt(
        const uint8_t* ciphertext,  size_t ciphertextSize,
        const uint8_t* tag,         size_t tagSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        AlgoResult&    result
    ) override;

private:
    // Internal static buffers — owned by this object
    uint8_t _outputBuf[ALGO_MAX_CIPHERTEXT_SIZE];
    uint64_t  _outputSize;
};
