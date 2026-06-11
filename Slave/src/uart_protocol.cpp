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
            Serial.println("CRC mismatch detected");
            Serial.println("CRC calculated: " + String(computeCRC8(buf, 2), HEX) + ", CRC received: " + String(buf[2], HEX));
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

        uint8_t startByte;
        if (!readExact(&startByte, 1)) continue;

        if (startByte != UART_START_BYTE) {
            while (_serial.available()) _serial.read();
            continue;
        }

        uint8_t hdr[5]; // [algo][dataHi][dataLo][keyLength][nonceLength]
        if (!readExact(hdr, sizeof(hdr))) continue;
        Serial.println("Received request header: algo=" + String(hdr[0], HEX) + ", dataSize=" + String((hdr[1] << 8) | hdr[2]) + ", keySize=" + String(hdr[3]) + ", nonceSize=" + String(hdr[4]));

        pkt.algorithm = static_cast<UartAlgorithm>(hdr[0]);
        pkt.dataSize = ((uint16_t)hdr[1] << 8) | hdr[2];
        pkt.keySize  = hdr[3];
        pkt.nonceSize = hdr[4];

        if (pkt.keySize > UART_MAX_KEY_SIZE || pkt.nonceSize > UART_MAX_NONCE_SIZE) {
            while (_serial.available()) _serial.read();
            sendAck(false);
            return UART_ERR_OVERFLOW;
        }

        pkt.data = new uint8_t[pkt.dataSize];
        if (pkt.dataSize > 0 && !readExact(pkt.data, pkt.dataSize)) continue;
        if (pkt.keySize  > 0 && !readExact(pkt.key,  pkt.keySize))  continue;
        if (pkt.nonceSize > 0 && !readExact(pkt.nonce, pkt.nonceSize)) continue;

        uint8_t receivedCrc;
        if (!readExact(&receivedCrc, 1)) continue;

        uint8_t calcCrc;
        calcCrc = computeCRC8(&startByte, 1);
        calcCrc = continueCRC8(calcCrc, hdr, sizeof(hdr));
        calcCrc = continueCRC8(calcCrc, pkt.data, pkt.dataSize);
        calcCrc = continueCRC8(calcCrc, pkt.key,  pkt.keySize);
        calcCrc = continueCRC8(calcCrc, pkt.nonce, pkt.nonceSize);

        if (calcCrc != receivedCrc) {
            // Serial.println("CRC calculated: " + String(calcCrc, HEX) + ", CRC received: " + String(receivedCrc, HEX));
            sendAck(false); // NACK: CRC mismatch
            // Serial.println(String(pkt.dataSize) + " bytes data, " + String(pkt.keySize) + " bytes key, " + String(pkt.nonceSize) + " bytes nonce");
            // Serial.println("Data (hex): " + String((char*)pkt.data, pkt.dataSize));
            continue;
        }

        sendAck(true);
        return UART_OK;
    }

    return UART_ERR_TIMEOUT;
}

UartStatus UartProtocol::slaveSendResponse(const ResponsePacket& pkt)
{
    // Build header: [J][timeHi][timeLo][dataSizeHi][dataSizeLo]
    uint8_t header[5];
    header[0] = UART_START_BYTE;
    header[1] = pkt.timeMs >> 8;
    header[2] = pkt.timeMs & 0xFF;
    header[3] = pkt.dataSize >> 8;
    header[4] = pkt.dataSize & 0xFF;

    uint8_t fullCrc;
    fullCrc = computeCRC8(header, sizeof(header));
    fullCrc = continueCRC8(fullCrc, pkt.data, pkt.dataSize);

    UartStatus status;
    for (int attempt = 0; attempt < UART_MAX_RETRIES; ++attempt) {
        while (_serial.available()) _serial.read();

        // Transmit: header | data | CRC
        _serial.write(header, sizeof(header));
        if (pkt.dataSize > 0) _serial.write(pkt.data, pkt.dataSize);
        _serial.write(&fullCrc, 1);

        status = receiveAck();

        if (status == UART_OK)
            return UART_OK;

        // NACK means master detected a CRC error; resend
        // UART_ERR_TIMEOUT means no reply at all; also resend
    }

    return status;
}
