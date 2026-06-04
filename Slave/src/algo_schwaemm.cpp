#include "algo_schwaemm.h"

// TODO: include your SCHWAEMM / Sparkle library header here, e.g.:
// #include <sparkle.h>

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

    // ── TODO: replace stub with real SCHWAEMM256-128 encrypt call ─────────
    //
    // Expected call signature (Sparkle reference implementation):
    //
    //   void schwaemm256_128_encrypt(
    //       uint8_t*       ciphertext,   // output (same length as plaintext)
    //       uint8_t*       tag,          // output (16 bytes)
    //       const uint8_t* plaintext,    size_t plaintextLen,
    //       const uint8_t* ad,           size_t adLen,
    //       const uint8_t* nonce,        // 32 bytes
    //       const uint8_t* key           // 16 bytes
    //   );
    //
    memcpy(_outputBuf, plaintext, plaintextSize);          // stub: echo plaintext
    memset(_tagBuf, 0xCC, this->tagSize());                // stub: dummy tag

    // ── Populate result ───────────────────────────────────────────────────
    result.output     = _outputBuf;
    // result.outputSize = plaintextSize;
    // result.tag        = _tagBuf;
    // result.tagSize    = this->tagSize();

    return ALGO_OK;
}
