#include "algo_aesgcm.h"

#include "Crypto.h"
#include "AES.h"
#include "GCM.h"

AlgoStatus AESGCMAlgorithm::decrypt(
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

    GCM<AES128> gcm;

    gcm.clear();
    gcm.setKey(key, keySize);
    gcm.setIV(nonce, nonceSize);
    gcm.decrypt(result, ciphertext, ciphertextSize-this->tagSize());

    // The received tag is appended to the ciphertext ([ct | tag]), not before `result`.
    if (!gcm.checkTag(ciphertext + ciphertextSize - this->tagSize(), this->tagSize())) return ALGO_ERR_AUTH_FAIL;

    if (resultSize != nullptr) {
        *resultSize = ciphertextSize-this->tagSize();
    }
    return ALGO_OK;
}