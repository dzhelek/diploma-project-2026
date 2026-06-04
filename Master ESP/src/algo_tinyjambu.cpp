#include "algo_tinyjambu.h"

// TODO: include your TinyJAMBU library header here, e.g.:
// #include <tinyjambu.h>

AlgoStatus TinyJAMBUAlgorithm::decrypt(
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
    if ((s = checkSize(tagSize,   this->tagSize(),   ALGO_ERR_AUTH_FAIL))     != ALGO_OK) return s;
    if (ciphertextSize > ALGO_MAX_CIPHERTEXT_SIZE) return ALGO_ERR_OVERFLOW;

    // ── TODO: replace stub with real TinyJAMBU-128 decrypt + verify ───────
    //
    // Expected call signature (reference implementation):
    //
    //   int tinyjambu_128_decrypt(
    //       uint8_t*       plaintext,    // output (same length as ciphertext)
    //       const uint8_t* ciphertext,   size_t ciphertextLen,
    //       const uint8_t* tag,          // 8 bytes — returns non-zero on fail
    //       const uint8_t* ad,           size_t adLen,
    //       const uint8_t* nonce,        // 12 bytes
    //       const uint8_t* key           // 16 bytes
    //   );
    //   // returns 0 on success, non-zero on tag mismatch
    //
    memcpy(_outputBuf, ciphertext, ciphertextSize);        // stub: echo ciphertext
    memcpy(_tagBuf,    tag,        tagSize);               // stub: copy tag as-is
    bool authOk = true;                                    // stub: always succeeds

    if (!authOk) return ALGO_ERR_AUTH_FAIL;

    result.output     = _outputBuf;
    result.outputSize = ciphertextSize;
    result.tag        = _tagBuf;
    result.tagSize    = tagSize;

    return ALGO_OK;
}
