#include "algo_tinyjambu.h"

#include "TinyJAMBU.h" 

AlgoStatus TinyJAMBUAlgorithm::decrypt(
    const uint8_t* ciphertext,  size_t ciphertextSize,
    const uint8_t* tag,         size_t tagSize,
    const uint8_t* key,         size_t keySize,
    const uint8_t* nonce,       size_t nonceSize,
    const uint8_t* ad,          size_t adSize,
    uint8_t* result,            size_t* resultSize
)
{
    AlgoStatus s;
    if ((s = checkSize(keySize,   this->keySize(),   ALGO_ERR_INVALID_KEY))   != ALGO_OK) return s;
    if ((s = checkSize(nonceSize, this->nonceSize(), ALGO_ERR_INVALID_NONCE)) != ALGO_OK) return s;

    int auth = tinyjambu_128_aead_decrypt(
        result, resultSize, ciphertext, ciphertextSize,
        ad, adSize,
        nonce, key
    );

    if (auth == -1) return ALGO_ERR_AUTH_FAIL;
    if (auth < -1) return ALGO_ERR_UNKNOWN;

    return ALGO_OK;
}
