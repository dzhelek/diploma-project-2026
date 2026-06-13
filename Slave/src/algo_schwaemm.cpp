#include "algo_schwaemm.h"

// SCHWAEMM's AEAD API is renamed to avoid a link-time clash with ASCON (both use
// the SUPERCOP names crypto_aead_encrypt/decrypt). Keep in sync with schwaemm.c.
#define crypto_aead_encrypt  schwaemm_aead_encrypt
#define crypto_aead_decrypt  schwaemm_aead_decrypt

// schwaemm.c is compiled as C (C linkage); this wrapper is C++. Include the header
// inside extern "C" so the call isn't name-mangled and matches the C definition.
extern "C" {
#include "schwaemm.h"
}

AlgoStatus SCHWAEMMAlgorithm::encrypt(
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
