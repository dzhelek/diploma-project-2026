#include "algo_ascon.h"

#include "crypto_aead.h"

AlgoStatus AsconAlgorithm::decrypt(
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

    unsigned long long resultLen;

    int auth = crypto_aead_decrypt(
        result, &resultLen, // plaintext output
        nullptr, ciphertext, ciphertextSize,
        ad, adSize,
        nonce, key
    );

    if (auth != 0) return ALGO_ERR_AUTH_FAIL;

    if (resultSize != nullptr) {
        *resultSize = static_cast<size_t>(resultLen);
    }
    return ALGO_OK;
}
