#include "algo_ascon.h"

#include "crypto_aead.h"

AlgoStatus AsconAlgorithm::encrypt(
    const uint8_t* plaintext,   size_t plaintextSize,
    const uint8_t* key,         size_t keySize,
    const uint8_t* nonce,       size_t nonceSize,
    const uint8_t* ad,          size_t adSize,
    uint8_t* result,            size_t* resultSize
)
{
    AlgoStatus s;
    if ((s = checkSize(keySize,   this->keySize(),   ALGO_ERR_INVALID_KEY))   != ALGO_OK) return s;
    if ((s = checkSize(nonceSize, this->nonceSize(), ALGO_ERR_INVALID_NONCE)) != ALGO_OK) return s;

    unsigned long long outLen;

    crypto_aead_encrypt(
        result, &outLen,
        plaintext, plaintextSize,
        ad, adSize,
        nullptr,
        nonce,
        key
    );

    if (resultSize)
        *resultSize = static_cast<size_t>(outLen);

    return ALGO_OK;
}
