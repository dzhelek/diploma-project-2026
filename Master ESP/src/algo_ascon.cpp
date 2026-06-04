#include "algo_ascon.h"

#include "crypto_aead.h"

AlgoStatus AsconAlgorithm::decrypt(
    const uint8_t* ciphertext,  size_t ciphertextSize,
    const uint8_t* tag,         size_t tagSize,
    const uint8_t* key,         size_t keySize,
    const uint8_t* nonce,       size_t nonceSize,
    const uint8_t* ad,          size_t adSize,
    AlgoResult&    result)
{
    // ── Validate parameters ───────────────────────────────────────────────
    AlgoStatus s;
    if ((s = checkSize(keySize,   this->keySize(),   ALGO_ERR_INVALID_KEY))   != ALGO_OK) return s;
    if ((s = checkSize(nonceSize, this->nonceSize(), ALGO_ERR_INVALID_NONCE)) != ALGO_OK) return s;
    // if ((s = checkSize(tagSize,   this->tagSize(),   ALGO_ERR_AUTH_FAIL))     != ALGO_OK) return s;
    if (ciphertextSize > ALGO_MAX_CIPHERTEXT_SIZE) return ALGO_ERR_OVERFLOW;

    crypto_aead_decrypt(
        _outputBuf, &_outputSize, // plaintext output
        nullptr, ciphertext, ciphertextSize,
        ad, adSize,
        nonce, key
    );

    // ── Populate result ───────────────────────────────────────────────────
    result.output     = _outputBuf;
    result.outputSize = _outputSize;
    // result.tag        = _tagBuf;
    // result.tagSize    = tagSize;

    return ALGO_OK;
}
