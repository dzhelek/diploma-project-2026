#include "algo_tinyjambu.h"

#include "TinyJAMBU.h"

AlgoStatus TinyJAMBUAlgorithm::encrypt(
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

    tinyjambu_128_aead_encrypt(
        result, resultSize,
        plaintext, plaintextSize,
        ad, adSize,
        nonce,
        key
    );

    return ALGO_OK;
}
