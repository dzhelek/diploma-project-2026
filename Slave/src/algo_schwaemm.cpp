#include "algo_schwaemm.h"

#include "schwaemm.h"

AlgoStatus SCHWAEMMAlgorithm::encrypt(
    const uint8_t* plaintext,  size_t plaintextSize,
    const uint8_t* key,        size_t keySize,
    const uint8_t* nonce,      size_t nonceSize,
    const uint8_t* ad,         size_t adSize,
    AlgoResult&    result)
{
    // ── Validate parameters ───────────────────────────────────────────────
    AlgoStatus s;
    if ((s = checkSize(keySize,   this->keySize(),   ALGO_ERR_INVALID_KEY))   != ALGO_OK) return s;
    if ((s = checkSize(nonceSize, this->nonceSize(), ALGO_ERR_INVALID_NONCE)) != ALGO_OK) return s;
    if (plaintextSize > ALGO_MAX_PLAINTEXT_SIZE) return ALGO_ERR_OVERFLOW;

    crypto_aead_encrypt(
        _outputBuf, &_outputSize,
        plaintext, plaintextSize,
        ad, adSize,
        nullptr,
        nonce,
        key
    );

    // ── Populate result ───────────────────────────────────────────────────
    result.output     = _outputBuf;
    result.outputSize = _outputSize;

    return ALGO_OK;
}
