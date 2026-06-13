#include "algo_aesgcm.h"

#include "Crypto.h"
#include "AES.h"
#include "GCM.h"

AlgoStatus AESGCMAlgorithm::encrypt(
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

    GCM<AES128> gcm;

    gcm.clear();
    gcm.setKey(key, keySize);
    gcm.setIV(nonce, nonceSize);
    gcm.encrypt(result, plaintext, plaintextSize);
    gcm.computeTag(result+plaintextSize, this->tagSize());

    if (resultSize)
        *resultSize = plaintextSize + this->tagSize();

    return ALGO_OK;
}
