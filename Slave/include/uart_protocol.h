#pragma once

#include <Arduino.h>
#include <stdint.h>

#define UART_START_BYTE       'J'
#define UART_TIMEOUT_MS       3000
#define UART_MAX_RETRIES      3
#define UART_MAX_KEY_SIZE     (24)
#define UART_MAX_NONCE_SIZE   (24)

enum UartAlgorithm : uint8_t {
    ALGO_ASCON      = 0xA1,
    ALGO_TINYJAMBU  = 0xA2,
    ALGO_SCHWAEMM   = 0xA3,
};

enum UartPacketType : uint8_t {
    PKT_HI       = 0xF0,
    PKT_ACK      = 0xF1,
    PKT_NACK     = 0xF2,
    PKT_REQUEST  = 0xF3,
    PKT_RESPONSE = 0xF4,
};

enum UartStatus : int8_t {
    UART_OK            =  0,  // Success
    UART_ERR_TIMEOUT   = -1,  // No reply within UART_TIMEOUT_MS * UART_MAX_RETRIES
    UART_ERR_CRC       = -2,  // CRC-8 mismatch
    UART_ERR_NACK      = -3,  // Peer sent NACK
    UART_ERR_BAD_START = -4,  // Start byte was not 'J'
    UART_ERR_OVERFLOW  = -5,  // Data or key larger than static buffers
    UART_ERR_UNKNOWN   = -6,  // Unexpected / malformed packet
};

/**
 * "hi" packet  –  master --> slave
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
 * Request packet  –  master --> slave
 *   [J][algorithm][data_size_hi][data_size_lo][key_size][nonce_size]
 *   [data_size bytes of data][key_size bytes of key][nonce_size bytes of nonce][CRC-8]
 */
struct RequestPacket {
    UartAlgorithm algorithm;
    uint16_t      dataSize;
    uint8_t       keySize;
    uint8_t       nonceSize;
    uint8_t*      data;
    uint8_t       key [UART_MAX_KEY_SIZE];
    uint8_t       nonce[UART_MAX_NONCE_SIZE];
};

/**
 * Response packet  –  slave --> master
 *   [J][time_hi][time_lo][data_size_hi][data_size_lo][data_size bytes of data][CRC-8]
 */
struct ResponsePacket {
    uint16_t timeMs;
    uint16_t dataSize;
    uint8_t* data;
};

class UartProtocol {
public:
    explicit UartProtocol(HardwareSerial& serial);

    // Compute CRC-8 (poly 0x07, init 0x00) over @p len bytes of @p data.
    static uint8_t computeCRC8(const uint8_t* data, size_t len);
    static uint8_t continueCRC8(uint8_t runningCrc, const uint8_t* data, size_t len);

    // Wait for a "hi" packet, then reply ACK or NACK.
    UartStatus slaveReceiveHi(HiPacket& pkt, UartAlgorithm* supportedAlgos, size_t supportedAlgoCount);
    // Wait for a request packet, then send ACK.
    UartStatus slaveReceiveRequest(RequestPacket& pkt);
    // Send a response packet and wait for ACK from the master.
    UartStatus slaveSendResponse(const ResponsePacket& pkt);

private:
    // Block until @p len bytes are available or timeout expires.
    bool waitForBytes(size_t len);
    // Send raw bytes followed by a CRC-8 byte computed over them.
    void sendWithCRC(const uint8_t* buf, size_t len);
    // Read exactly @p len bytes into @p buf; returns false on timeout.
    bool readExact(uint8_t* buf, size_t len);
    // Transmit an ACK or NACK packet.
    void sendAck(bool ack);
    // Receive an ACK or NACK packet.
    UartStatus receiveAck();

    HardwareSerial& _serial;
};
