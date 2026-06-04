#pragma once

#include <Arduino.h>
#include <stdint.h>

// ─────────────────────────────────────────────
//  Compile-time buffer limits
// ─────────────────────────────────────────────

#define ALGO_MAX_PLAINTEXT_SIZE  (16 * 1024) // 16 KB
#define ALGO_MAX_CIPHERTEXT_SIZE (16 * 1024) // same length as plaintext
#define ALGO_MAX_TAG_SIZE        64           // bytes; enough for all variants
#define ALGO_MAX_NONCE_SIZE      64           // bytes
#define ALGO_MAX_AD_SIZE         (4 * 1024)  // 4 KB associated data

// ─────────────────────────────────────────────
//  Return codes
// ─────────────────────────────────────────────

enum AlgoStatus : int8_t {
    ALGO_OK               =  0,  // Success
    ALGO_ERR_INVALID_KEY  = -1,  // Key size not accepted by this algorithm
    ALGO_ERR_INVALID_NONCE= -2,  // Nonce size not accepted by this algorithm
    ALGO_ERR_OVERFLOW     = -3,  // Input exceeds internal buffer
    ALGO_ERR_AUTH_FAIL    = -4,  // Tag verification failed during decrypt
    ALGO_ERR_NOT_IMPL     = -5,  // Algorithm not implemented on this device
    ALGO_ERR_UNKNOWN      = -6,  // Unspecified internal error
};

// ─────────────────────────────────────────────
//  AlgoResult  –  output of encrypt / decrypt
// ─────────────────────────────────────────────

/**
 * Owns the output buffers for one encrypt/decrypt call.
 * Memory is managed by the concrete algorithm object; the caller must
 * not free or outlive the algorithm instance while using this struct.
 */
struct AlgoResult {
    uint8_t* output;      // Ciphertext (encrypt) or plaintext (decrypt)
    size_t   outputSize;  // Number of valid bytes in output[]

    uint8_t* tag;         // Authentication tag (encrypt only;
                          // on decrypt this is the verified tag copy)
    size_t   tagSize;     // Number of valid bytes in tag[]
};

// ─────────────────────────────────────────────
//  IAlgorithm  –  abstract interface
// ─────────────────────────────────────────────

/**
 * Pure-virtual interface that every AEAD algorithm must implement.
 *
 * Ownership rules
 * ───────────────
 * - The concrete class owns all internal buffers (output, tag).
 * - AlgoResult::output and AlgoResult::tag point into those buffers.
 * - The pointed-to data is valid until the next call to encrypt(),
 *   decrypt(), or destruction of the object.
 * - The caller must NOT free these pointers.
 *
 * Thread / reentrancy
 * ───────────────────
 * Not thread-safe; calls must be serialised by the caller (same
 * constraint as the UART protocol layer).
 */
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;

    // ── Identity ──────────────────────────────────────────────────────────

    /** Human-readable name, e.g. "Ascon-128". */
    virtual const char* name() const = 0;

    // ── Parameter constraints ─────────────────────────────────────────────

    /** Expected key size in bytes (0 = variable / unchecked). */
    virtual size_t keySize()   const = 0;

    /** Expected nonce size in bytes (0 = variable / unchecked). */
    virtual size_t nonceSize() const = 0;

    /** Tag size in bytes produced / expected by this algorithm. */
    virtual size_t tagSize()   const = 0;

    /**
     * Decrypt ciphertext and verify authentication tag.
     *
     * @param ciphertext      Input ciphertext bytes.
     * @param ciphertextSize  Number of ciphertext bytes.
     * @param tag             Authentication tag to verify.
     * @param tagSize         Number of tag bytes.
     * @param key             Secret key bytes.
     * @param keySize         Number of key bytes.
     * @param nonce           Nonce / IV bytes.
     * @param nonceSize       Number of nonce bytes.
     * @param ad              Associated data bytes (may be nullptr if adSize==0).
     * @param adSize          Number of associated data bytes (may be 0).
     * @param[out] result     Populated on ALGO_OK; pointers owned by this object.
     *                        result.tag holds a copy of the verified tag.
     *
     * @return ALGO_OK on success, ALGO_ERR_AUTH_FAIL if tag mismatch,
     *         other error code on input/buffer problems.
     */
    virtual AlgoStatus decrypt(
        const uint8_t* ciphertext,  size_t ciphertextSize,
        const uint8_t* tag,         size_t tagSize,
        const uint8_t* key,         size_t keySize,
        const uint8_t* nonce,       size_t nonceSize,
        const uint8_t* ad,          size_t adSize,
        AlgoResult&    result
    ) = 0;

protected:
    // ── Shared helpers available to all subclasses ────────────────────────

    /**
     * Validate that @p actual matches the expected size for this algorithm.
     * Pass 0 as @p expected to skip the check (variable-size parameter).
     */
    static AlgoStatus checkSize(size_t actual, size_t expected,
                                AlgoStatus errCode)
    {
        if (expected != 0 && actual != expected) return errCode;
        return ALGO_OK;
    }
};

// ─────────────────────────────────────────────
//  AlgorithmFactory
// ─────────────────────────────────────────────

#include "uart_protocol.h" // for UartAlgorithm enum

/**
 * Returns a pointer to the singleton instance for a given UartAlgorithm.
 * Returns nullptr if the algorithm is not implemented on this device.
 *
 * The returned pointer is valid for the lifetime of the program; do NOT
 * delete it.
 */
IAlgorithm* AlgorithmFactory(UartAlgorithm algo);
