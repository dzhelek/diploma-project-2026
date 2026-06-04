#include "uart_protocol.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

UartProtocol::UartProtocol(HardwareSerial& serial) : _serial(serial)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  CRC-8  (polynomial 0x07, init 0x00)
// ─────────────────────────────────────────────────────────────────────────────

uint8_t UartProtocol::computeCRC8(const uint8_t* data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────

bool UartProtocol::waitForBytes(size_t len)
{
    uint32_t deadline = millis() + UART_TIMEOUT_MS;
    while ((size_t)_serial.available() < len) {
        if (millis() > deadline)
            return false;
        delay(1);
    }
    return true;
}

bool UartProtocol::readExact(uint8_t* buf, size_t len)
{
    size_t received = 0;
    uint32_t deadline = millis() + UART_TIMEOUT_MS;

    while (received < len) {
        if (millis() > deadline)
            return false;
        if (_serial.available()) {
            buf[received++] = (uint8_t)_serial.read();
        } else {
            delay(1);
        }
    }
    return true;
}

void UartProtocol::sendWithCRC(const uint8_t* buf, size_t len)
{
    _serial.write(buf, len);
    uint8_t crc = computeCRC8(buf, len);
    _serial.write(&crc, 1);
}

void UartProtocol::sendAck(bool ack)
{
    // [J][0x01 or 0x00][CRC-8]
    uint8_t buf[2] = { UART_START_BYTE, static_cast<uint8_t>(ack ? 1u : 0u) };
    sendWithCRC(buf, sizeof(buf));
}

UartStatus UartProtocol::receiveAck()
{
    // Expect [J][value][CRC-8]  = 3 bytes
    uint8_t buf[3];

    if (!readExact(buf, sizeof(buf)))
        return UART_ERR_TIMEOUT;

    // Validate start byte
    if (buf[0] != UART_START_BYTE)
        return UART_ERR_BAD_START;

    // Validate CRC over first two bytes
    if (computeCRC8(buf, 2) != buf[2])
        return UART_ERR_CRC;

    return (buf[1] == 0x01) ? UART_OK : UART_ERR_NACK;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slave API
// ─────────────────────────────────────────────────────────────────────────────

UartStatus UartProtocol::slaveReceiveHi(HiPacket& pkt, UartAlgorithm* supportedAlgos, size_t supportedAlgoCount)
{
    // Packet: [J][algorithm][CRC-8]  = 3 bytes

    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {
        uint8_t buf[3];
        if (!readExact(buf, sizeof(buf))) continue; // timeout → retry

        if (buf[0] != UART_START_BYTE) {
            while (_serial.available()) _serial.read();
            continue;
        }

        if (computeCRC8(buf, 2) != buf[2]) {
            sendAck(false); // NACK: bad CRC
            continue;
        }

        pkt.algorithm = static_cast<UartAlgorithm>(buf[1]);

        bool algoSupported = false;
        for (size_t i = 0; i < supportedAlgoCount; ++i) {
            if (supportedAlgos[i] == pkt.algorithm) {
                algoSupported = true;
                break;
            }
        }

        sendAck(algoSupported);
        return algoSupported ? UART_OK : UART_ERR_NACK;
    }

    return UART_ERR_TIMEOUT;
}

UartStatus UartProtocol::slaveReceiveRequest(RequestPacket& pkt)
{
    // Packet: [J][algo][dataHi][dataLo][keyLength][nonceLength][data...][key...][nonce...][CRC-8]

    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {

        // ── 1. Start byte ───────────────────────────────────────────────
        uint8_t startByte;
        if (!readExact(&startByte, 1)) continue;

        if (startByte != UART_START_BYTE) {
            while (_serial.available()) _serial.read();
            continue;
        }

        // ── 2. Fixed header ─────────────────────────────────────────────
        uint8_t hdr[5]; // [algo][dataHi][dataLo][keyLength][nonceLength]
        if (!readExact(hdr, sizeof(hdr))) continue;

        uint16_t dataSize = ((uint16_t)hdr[1] << 8) | hdr[2];
        uint8_t keySize  = hdr[3];
        uint8_t nonceSize = hdr[4];

        if (dataSize > UART_MAX_DATA_SIZE || keySize > UART_MAX_KEY_SIZE || nonceSize > UART_MAX_NONCE_SIZE) {
            while (_serial.available()) _serial.read();
            sendAck(false);
            return UART_ERR_OVERFLOW;
        }

        // ── 3. Variable data ────────────────────────────────────────────
        if (dataSize > 0 && !readExact(pkt.data, dataSize)) continue;
        if (keySize  > 0 && !readExact(pkt.key,  keySize))  continue;
        if (nonceSize > 0 && !readExact(pkt.nonce, nonceSize)) continue;

        // ── 4. CRC byte ─────────────────────────────────────────────────
        uint8_t receivedCrc;
        if (!readExact(&receivedCrc, 1)) continue;

        // ── 5. Verify CRC ───────────────────────────────────────────────
        // Rebuild the full transmitted header as seen on wire: [J][algo][...]
        uint8_t fullHdr[6];
        fullHdr[0] = startByte;
        fullHdr[1] = hdr[0]; // algo
        fullHdr[2] = hdr[1]; fullHdr[3] = hdr[2]; // dataSize
        fullHdr[4] = hdr[3]; fullHdr[5] = hdr[4]; // keySize

        auto crc8_continue = [](uint8_t runningCrc,
                                const uint8_t* data, size_t len) -> uint8_t {
            for (size_t i = 0; i < len; ++i) {
                runningCrc ^= data[i];
                for (uint8_t bit = 0; bit < 8; ++bit) {
                    if (runningCrc & 0x80)
                        runningCrc = (runningCrc << 1) ^ 0x07;
                    else
                        runningCrc <<= 1;
                }
            }
            return runningCrc;
        };

        uint8_t calcCrc = 0x00;
        calcCrc = crc8_continue(calcCrc, fullHdr,  sizeof(fullHdr));
        calcCrc = crc8_continue(calcCrc, pkt.data, dataSize);
        calcCrc = crc8_continue(calcCrc, pkt.key,  keySize);
        calcCrc = crc8_continue(calcCrc, pkt.nonce, nonceSize);
        if (calcCrc != receivedCrc) {
            sendAck(false); // NACK: CRC mismatch
            continue;
        }

        // ── 6. Populate struct and ACK ──────────────────────────────────
        pkt.algorithm = static_cast<UartAlgorithm>(hdr[0]);
        pkt.dataSize  = dataSize;
        pkt.keySize   = keySize;
        pkt.nonceSize = nonceSize;
        sendAck(true);
        return UART_OK;
    }

    return UART_ERR_TIMEOUT;
}

UartStatus UartProtocol::slaveSendResponse(const ResponsePacket& pkt)
{
    if (pkt.dataSize > UART_MAX_DATA_SIZE) return UART_ERR_OVERFLOW;

    // Build header: [J][timeHi][timeLo][dataSizeHi][dataSizeLo]
    uint8_t header[5];
    header[0] = UART_START_BYTE;
    header[1] = static_cast<uint8_t>(pkt.timeMs   >> 8);
    header[2] = static_cast<uint8_t>(pkt.timeMs   & 0xFF);
    header[3] = static_cast<uint8_t>(pkt.dataSize >> 8);
    header[4] = static_cast<uint8_t>(pkt.dataSize & 0xFF);

    // Compute CRC over header + data
    auto crc8_continue = [](uint8_t runningCrc,
                            const uint8_t* data, size_t len) -> uint8_t {
        for (size_t i = 0; i < len; ++i) {
            runningCrc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit) {
                if (runningCrc & 0x80)
                    runningCrc = (runningCrc << 1) ^ 0x07;
                else
                    runningCrc <<= 1;
            }
        }
        return runningCrc;
    };

    uint8_t fullCrc = 0x00;
    fullCrc = crc8_continue(fullCrc, header,   sizeof(header));
    fullCrc = crc8_continue(fullCrc, pkt.data, pkt.dataSize);

    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {
        while (_serial.available()) _serial.read();

        // Transmit: header | data | CRC
        _serial.write(header, sizeof(header));
        if (pkt.dataSize > 0) _serial.write(pkt.data, pkt.dataSize);
        _serial.write(&fullCrc, 1);

        UartStatus status = receiveAck();

        if (status == UART_OK)
            return UART_OK;

        // NACK means master detected a CRC error; resend
        // UART_ERR_TIMEOUT means no reply at all; also resend
    }

    return UART_ERR_TIMEOUT;
}
