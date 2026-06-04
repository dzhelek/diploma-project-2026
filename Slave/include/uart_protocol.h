#pragma once

#include <Arduino.h>
#include <stdint.h>

// ─────────────────────────────────────────────
//  Compile-time constants
// ─────────────────────────────────────────────

#define UART_START_BYTE       'J'         // Magic / start byte for every packet
#define UART_TIMEOUT_MS       3000        // Per-attempt receive timeout (ms)
#define UART_MAX_RETRIES      3           // Number of retries before giving up
#define UART_MAX_DATA_SIZE    (16 * 1024)     // 16 KB
#define UART_MAX_KEY_SIZE     (32)        // 32 bytes
#define UART_MAX_NONCE_SIZE   (32)        // 32 bytes

// ─────────────────────────────────────────────
//  Enumerations
// ─────────────────────────────────────────────

/**
 * Algorithm identifiers.
 * The slave must reject (NACK) any algorithm it does not support.
 */
enum UartAlgorithm : uint8_t {
    ALGO_ASCON      = 0x01,
    ALGO_TINYJAMBU  = 0x02,
    ALGO_SCHWAEMM   = 0x03,
};

/**
 * Packet type identifiers (internal use; not transmitted as a field,
 * inferred from context / parser state).
 */
enum UartPacketType : uint8_t {
    PKT_HI       = 0xA0,
    PKT_ACK      = 0xA1,
    PKT_NACK     = 0xA2,
    PKT_REQUEST  = 0xA3,
    PKT_RESPONSE = 0xA4,
};

/**
 * Return codes used by every public function.
 */
enum UartStatus : int8_t {
    UART_OK            =  0,  // Success
    UART_ERR_TIMEOUT   = -1,  // No reply within UART_TIMEOUT_MS * UART_MAX_RETRIES
    UART_ERR_CRC       = -2,  // CRC-8 mismatch
    UART_ERR_NACK      = -3,  // Peer sent NACK
    UART_ERR_BAD_START = -4,  // Start byte was not 'J'
    UART_ERR_OVERFLOW  = -5,  // Data or key larger than static buffers
    UART_ERR_UNKNOWN   = -6,  // Unexpected / malformed packet
};

// ─────────────────────────────────────────────
//  Packet structures
// ─────────────────────────────────────────────

/**
 * "hi" packet  –  master → slave
 *   [J][algorithm][CRC-8]
 */
struct HiPacket {
    UartAlgorithm algorithm;
};

/**
 * ACK / NACK packet  –  bidirectional
 *   [J][0x00=NACK | 0x01=ACK][CRC-8]
 */
struct AckPacket {
    bool ack; // true = ACK, false = NACK
};

/**
 * Request packet  –  master → slave
 *   [J][algorithm][data_size_hi][data_size_lo][key_size_hi][key_size_lo]
 *   [data_size bytes of data][key_size bytes of key][CRC-8]
 *
 * The library owns the data/key buffers; pointers remain valid until the
 * next call that modifies this struct.
 */
struct RequestPacket {
    UartAlgorithm algorithm;
    uint16_t      dataSize;                  // Number of valid bytes in data[]
    uint8_t       keySize;                   // Number of valid bytes in key[]
    uint8_t       nonceSize;                 // Number of valid bytes in nonce[]
    uint8_t       data[UART_MAX_DATA_SIZE];
    uint8_t       key [UART_MAX_KEY_SIZE];
    uint8_t       nonce[UART_MAX_NONCE_SIZE];
};

/**
 * Response packet  –  slave → master
 *   [J][time_hi][time_lo][data_size_hi][data_size_lo][data_size bytes of data][CRC-8]
 *
 * time  – processing time in milliseconds (uint16_t, max ~65 s).
 * The library owns the data buffer.
 */
struct ResponsePacket {
    uint16_t timeMs;                         // Processing time in ms
    uint16_t dataSize;                       // Number of valid bytes in data[]
    uint8_t  data[UART_MAX_DATA_SIZE];
};

// ─────────────────────────────────────────────
//  UartProtocol class
// ─────────────────────────────────────────────

/**
 * Blocking UART protocol driver for ESP32 (Arduino framework, Serial2).
 *
 * Master flow
 * ──────────────────────────────────────────────
 *   1. masterSendHi()        → sends "hi", waits for ACK/NACK
 *   2. masterSendRequest()   → sends request, waits for ACK/NACK
 *   3. masterReceiveResponse()→ waits for response, sends ACK
 *
 * Slave flow
 * ──────────────────────────────────────────────
 *   1. slaveReceiveHi()      → waits for "hi", replies ACK/NACK
 *   2. slaveReceiveRequest() → waits for request, replies ACK
 *   3. slaveSendResponse()   → sends response, waits for ACK
 *
 * All functions are blocking and perform up to UART_MAX_RETRIES retries
 * before returning UART_ERR_TIMEOUT.
 */
class UartProtocol {
public:
    // ── Construction ──────────────────────────────────────────────────────

    /**
     * @param serial   Reference to the HardwareSerial port to use (Serial2).
     */
    explicit UartProtocol(HardwareSerial& serial);

    // ── CRC ───────────────────────────────────────────────────────────────

    /**
     * Compute CRC-8 (poly 0x07, init 0x00) over @p len bytes of @p data.
     * Made public so callers can verify independently.
     */
    static uint8_t computeCRC8(const uint8_t* data, size_t len);

    // ── Slave API ─────────────────────────────────────────────────────────

    /**
     * Wait for a "hi" packet, then reply ACK or NACK.
     *
     * @param[out] pkt              Populated with the received hi packet.
     * @param      supportedAlgos    Array of supported algorithms.
     * @param      supportedAlgoCount Number of supported algorithms.
     * @return UART_OK          – hi received and ACK sent.
     *         UART_ERR_NACK    – hi received but NACK sent (unsupported algo).
     *         UART_ERR_TIMEOUT – nothing received in time.
     *         UART_ERR_CRC     – packet failed CRC check.
     */
    UartStatus slaveReceiveHi(HiPacket& pkt, UartAlgorithm* supportedAlgos, size_t supportedAlgoCount);

    /**
     * Wait for a request packet, then send ACK.
     *
     * @param[out] pkt  Populated with the received request on UART_OK.
     * @return UART_OK on success, error code otherwise.
     */
    UartStatus slaveReceiveRequest(RequestPacket& pkt);

    /**
     * Send a response packet and wait for ACK from the master.
     *
     * @param pkt  The response packet to send (data must be filled).
     * @return UART_OK on success, error code otherwise.
     */
    UartStatus slaveSendResponse(const ResponsePacket& pkt);

private:
    // ── Internal helpers ──────────────────────────────────────────────────

    /** Block until @p len bytes are available or timeout expires.
     *  Returns true if all bytes arrived in time. */
    bool     waitForBytes(size_t len);

    /** Send raw bytes followed by a CRC-8 byte computed over them. */
    void     sendWithCRC(const uint8_t* buf, size_t len);

    /** Read exactly @p len bytes into @p buf; returns false on timeout. */
    bool     readExact(uint8_t* buf, size_t len);

    /** Transmit an ACK or NACK packet. */
    void     sendAck(bool ack);

    /**
     * Receive an ACK/NACK packet (with CRC).
     * Returns UART_OK / UART_ERR_NACK / UART_ERR_TIMEOUT / UART_ERR_CRC.
     */
    UartStatus receiveAck();

    // ── Members ───────────────────────────────────────────────────────────

    HardwareSerial& _serial;
};
