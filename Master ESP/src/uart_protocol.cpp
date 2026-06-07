#include "uart_protocol.h"

UartProtocol::UartProtocol(HardwareSerial& serial) : _serial(serial)
{}

uint8_t UartProtocol::computeCRC8(const uint8_t* data, size_t len)
{
    return continueCRC8(0x00, data, len);
}

uint8_t UartProtocol::continueCRC8(uint8_t runningCrc, const uint8_t* data, size_t len)
{
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
}

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

UartStatus UartProtocol::masterSendHi(const HiPacket& pkt)
{
    // Packet layout: [J][algorithm][CRC-8]
    uint8_t buf[2] = { UART_START_BYTE, static_cast<uint8_t>(pkt.algorithm) };

    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {
        // Flush stale incoming data before sending
        while (_serial.available()) _serial.read();

        sendWithCRC(buf, sizeof(buf));

        UartStatus status = receiveAck();

        if (status == UART_OK || status == UART_ERR_NACK)
            return status; // Definitive answer from slave; stop retrying

        // UART_ERR_TIMEOUT, UART_ERR_CRC, UART_ERR_BAD_START --> retry
    }

    return UART_ERR_TIMEOUT;
}

UartStatus UartProtocol::masterSendRequest(const RequestPacket& pkt)
{
    if (pkt.keySize  > UART_MAX_KEY_SIZE)  return UART_ERR_OVERFLOW;

    // Build header: [J][algo][dataHi][dataLo][keyLength][nonceLength]
    uint8_t header[6];
    header[0] = UART_START_BYTE;
    header[1] = static_cast<uint8_t>(pkt.algorithm);
    header[2] = static_cast<uint8_t>(pkt.dataSize >> 8);
    header[3] = static_cast<uint8_t>(pkt.dataSize & 0xFF);
    header[4] = static_cast<uint8_t>(pkt.keySize);
    header[5] = static_cast<uint8_t>(pkt.nonceSize);

    uint8_t fullCrc = 0x00;
    fullCrc = computeCRC8(header, sizeof(header));
    fullCrc = continueCRC8(fullCrc, pkt.data, pkt.dataSize);
    fullCrc = continueCRC8(fullCrc, pkt.key,  pkt.keySize);
    fullCrc = continueCRC8(fullCrc, pkt.nonce, pkt.nonceSize);


    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {
        Serial.println("Sending request: dataSize=" + String(pkt.dataSize) + ", keySize=" + String(pkt.keySize) + ", nonceSize=" + String(pkt.nonceSize));
        while (_serial.available()) _serial.read();

        // Send header, then data, then key, then CRC
        _serial.write(header, sizeof(header));
        if (pkt.dataSize > 0) _serial.write(pkt.data, pkt.dataSize);
        if (pkt.keySize  > 0) _serial.write(pkt.key,  pkt.keySize);
        if (pkt.nonceSize > 0) _serial.write(pkt.nonce, pkt.nonceSize);
        _serial.write(&fullCrc, 1);

        UartStatus status = receiveAck();

        if (status == UART_OK || status == UART_ERR_NACK)
            return status;
    }

    return UART_ERR_TIMEOUT;
}

UartStatus UartProtocol::masterReceiveResponse(ResponsePacket& pkt)
{
    // Fixed header after start byte: [timeHi][timeLo][dataSizeHi][dataSizeLo]
    // Total fixed portion transmitted: [J] + 4 bytes + n bytes data + [CRC]

    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {

        uint8_t startByte;
        if (!readExact(&startByte, 1)) continue; // timeout --> retry

        if (startByte != UART_START_BYTE) {
            while (_serial.available()) _serial.read();
            continue;
        }

        uint8_t hdr[4];
        if (!readExact(hdr, sizeof(hdr))) continue;

        pkt.timeMs   = ((uint16_t)hdr[0] << 8) | hdr[1];
        pkt.dataSize = ((uint16_t)hdr[2] << 8) | hdr[3];

        pkt.data = new uint8_t[pkt.dataSize];

        if (pkt.dataSize > 0 && !readExact(pkt.data, pkt.dataSize)) continue;

        uint8_t receivedCrc;
        if (!readExact(&receivedCrc, 1)) continue;

        uint8_t calcCrc = 0x00;
        calcCrc = computeCRC8(&startByte, 1);
        calcCrc = continueCRC8(calcCrc, hdr, sizeof(hdr));
        calcCrc = continueCRC8(calcCrc, pkt.data, pkt.dataSize);

        if (calcCrc != receivedCrc) {
            sendAck(false); // NACK to signal corruption
            continue;       // retry
        }

        sendAck(true);
        return UART_OK;
    }

    return UART_ERR_TIMEOUT;
}
