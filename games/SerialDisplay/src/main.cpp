#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <U8g2lib.h>

#include "EscapeKey.h"

namespace {
constexpr uint8_t OLED_CS = 5;
constexpr uint8_t OLED_DC = 16;
constexpr uint8_t OLED_RST = 17;
constexpr uint8_t SD_CS = 4;
constexpr uint32_t SERIAL_BAUD = 921600;
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint16_t SCREEN_HEIGHT = 64;
constexpr size_t FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT / 8;
constexpr size_t MAX_PAYLOAD = 4096;
constexpr size_t SERIAL_RX_BUFFER = 8192;
constexpr size_t SERIAL_READ_CHUNK = 512;
constexpr uint32_t PACKET_TIMEOUT_MS = 500;
constexpr uint32_t EXIT_HOLD_MS = 1000;
constexpr char MAIN_FIRMWARE[] = "/main.bin";
constexpr uint8_t MAGIC[4] = {'S', 'C', 'D', '1'};
constexpr uint8_t PROTOCOL_VERSION = 1;

enum class Command : uint8_t {
    Hello = 0x01,
    FrameRaw = 0x02,
    FrameRle = 0x03,
    Clear = 0x04,
    Contrast = 0x05,
    Ping = 0x06,
    Exit = 0x07,
    Ack = 0x80,
    Info = 0x81,
    Nack = 0xFF
};

enum class ErrorCode : uint8_t {
    None = 0,
    BadVersion = 1,
    PayloadTooLarge = 2,
    BadCrc = 3,
    BadLength = 4,
    BadRle = 5,
    UnknownCommand = 6
};

enum class ReceiverState : uint8_t {
    Magic,
    Metadata,
    Payload,
    Crc
};

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI display(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);
EscapeKey escapeKey;
uint8_t frameBuffer[FRAME_BYTES] = {};
uint8_t packetPayload[MAX_PAYLOAD] = {};
uint8_t serialReadBuffer[SERIAL_READ_CHUNK] = {};
uint8_t metadata[6] = {};
uint8_t crcBytes[2] = {};
ReceiverState receiverState = ReceiverState::Magic;
size_t magicIndex = 0;
size_t metadataIndex = 0;
size_t payloadIndex = 0;
size_t crcIndex = 0;
uint16_t payloadLength = 0;
uint16_t packetSequence = 0;
uint8_t packetCommand = 0;
uint32_t lastPacketByteAt = 0;
uint32_t returnAt = 0;
uint32_t frameCount = 0;
uint32_t errorCount = 0;
bool sdReady = false;
bool hostConnected = false;
bool hasFrame = false;
bool exitTriggered = false;

uint16_t crc16Update(uint16_t crc, uint8_t value) {
    crc ^= static_cast<uint16_t>(value) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000U) ? static_cast<uint16_t>((crc << 1) ^ 0x1021U)
                              : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

uint16_t packetCrc(const uint8_t *header, const uint8_t *payload, size_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t index = 0; index < sizeof(metadata); ++index) crc = crc16Update(crc, header[index]);
    for (size_t index = 0; index < length; ++index) crc = crc16Update(crc, payload[index]);
    return crc;
}

void writeLe16(uint8_t *target, uint16_t value) {
    target[0] = static_cast<uint8_t>(value & 0xFFU);
    target[1] = static_cast<uint8_t>(value >> 8);
}

void writeLe32(uint8_t *target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value & 0xFFU);
    target[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    target[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    target[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

void sendPacket(Command command, uint16_t sequence, const uint8_t *payload, uint16_t length) {
    uint8_t header[6];
    header[0] = PROTOCOL_VERSION;
    header[1] = static_cast<uint8_t>(command);
    writeLe16(header + 2, sequence);
    writeLe16(header + 4, length);
    const uint16_t crc = packetCrc(header, payload, length);
    const uint8_t checksum[2] = {
        static_cast<uint8_t>(crc & 0xFFU),
        static_cast<uint8_t>(crc >> 8)
    };
    Serial.write(MAGIC, sizeof(MAGIC));
    Serial.write(header, sizeof(header));
    if (length) Serial.write(payload, length);
    Serial.write(checksum, sizeof(checksum));
}

void sendAck(uint16_t sequence, uint8_t requestCommand) {
    const uint8_t payload[2] = {requestCommand, static_cast<uint8_t>(ErrorCode::None)};
    sendPacket(Command::Ack, sequence, payload, sizeof(payload));
}

void sendNack(uint16_t sequence, uint8_t requestCommand, ErrorCode error) {
    const uint8_t payload[2] = {requestCommand, static_cast<uint8_t>(error)};
    sendPacket(Command::Nack, sequence, payload, sizeof(payload));
}

void sendInfo(uint16_t sequence) {
    uint8_t payload[14] = {};
    writeLe16(payload, SCREEN_WIDTH);
    writeLe16(payload + 2, SCREEN_HEIGHT);
    payload[4] = 1;       // Horizontal LSB-first 1-bpp bitmap.
    payload[5] = 0x03;    // Raw and RLE frame support.
    writeLe32(payload + 6, SERIAL_BAUD);
    writeLe16(payload + 10, MAX_PAYLOAD);
    payload[12] = PROTOCOL_VERSION;
    payload[13] = 3;      // USB Display firmware revision.
    sendPacket(Command::Info, sequence, payload, sizeof(payload));
}

void drawWaitingScreen(const char *state) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 10, "USB DISPLAY");
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(2, 23, "SCREEN 256x64 MONO");
    display.drawStr(2, 33, "SERIAL 921600  PROTO SCD1");
    display.drawStr(2, 44, state);
    display.drawStr(2, 55, sdReady ? "SD ready" : "SD unavailable");
    display.drawStr(164, 63, "Hold ESC");
    display.sendBuffer();
}

void displayFrame() {
    uint8_t *displayBuffer = display.getBufferPtr();
    constexpr size_t inputStride = SCREEN_WIDTH / 8;
    for (size_t tileY = 0; tileY < SCREEN_HEIGHT / 8; ++tileY) {
        for (size_t byteX = 0; byteX < inputStride; ++byteX) {
            const size_t outputBase = tileY * SCREEN_WIDTH + byteX * 8;
            for (uint8_t bitX = 0; bitX < 8; ++bitX) {
                uint8_t column = 0;
                for (uint8_t bitY = 0; bitY < 8; ++bitY) {
                    const size_t inputIndex = (tileY * 8 + bitY) * inputStride + byteX;
                    column |= ((frameBuffer[inputIndex] >> bitX) & 1U) << bitY;
                }
                displayBuffer[outputBase + bitX] = column;
            }
        }
    }
    display.sendBuffer();
    ++frameCount;
    hasFrame = true;
}

bool decodeRle(const uint8_t *input, size_t length) {
    if (!length || (length & 1U)) return false;
    size_t output = 0;
    for (size_t index = 0; index < length; index += 2) {
        const uint8_t count = input[index];
        if (!count || output + count > FRAME_BYTES) return false;
        memset(frameBuffer + output, input[index + 1], count);
        output += count;
    }
    return output == FRAME_BYTES;
}

void handlePacket(uint8_t commandValue, uint16_t sequence, const uint8_t *payload, uint16_t length) {
    const Command command = static_cast<Command>(commandValue);
    switch (command) {
        case Command::Hello:
            if (length) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            hostConnected = true;
            if (!hasFrame) drawWaitingScreen("Host connected");
            sendInfo(sequence);
            return;
        case Command::FrameRaw:
            if (length != FRAME_BYTES) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            memcpy(frameBuffer, payload, FRAME_BYTES);
            displayFrame();
            sendAck(sequence, commandValue);
            return;
        case Command::FrameRle:
            if (!decodeRle(payload, length)) {
                sendNack(sequence, commandValue, ErrorCode::BadRle);
                return;
            }
            displayFrame();
            sendAck(sequence, commandValue);
            return;
        case Command::Clear:
            if (length != 1) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            memset(frameBuffer, payload[0] ? 0xFF : 0x00, FRAME_BYTES);
            displayFrame();
            sendAck(sequence, commandValue);
            return;
        case Command::Contrast:
            if (length != 1) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            display.setContrast(payload[0]);
            sendAck(sequence, commandValue);
            return;
        case Command::Ping:
            if (length) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            sendAck(sequence, commandValue);
            return;
        case Command::Exit:
            if (length) {
                sendNack(sequence, commandValue, ErrorCode::BadLength);
                return;
            }
            sendAck(sequence, commandValue);
            Serial.flush();
            returnAt = millis() + 150;
            return;
        default:
            sendNack(sequence, commandValue, ErrorCode::UnknownCommand);
            return;
    }
}

void resetReceiver() {
    receiverState = ReceiverState::Magic;
    magicIndex = 0;
    metadataIndex = 0;
    payloadIndex = 0;
    crcIndex = 0;
    payloadLength = 0;
}

void consumeSerialByte(uint8_t value) {
    switch (receiverState) {
        case ReceiverState::Magic:
            if (value == MAGIC[magicIndex]) {
                ++magicIndex;
                if (magicIndex == sizeof(MAGIC)) {
                    receiverState = ReceiverState::Metadata;
                    metadataIndex = 0;
                }
            } else {
                magicIndex = value == MAGIC[0] ? 1 : 0;
            }
            return;
        case ReceiverState::Metadata:
            metadata[metadataIndex++] = value;
            if (metadataIndex != sizeof(metadata)) return;
            packetCommand = metadata[1];
            packetSequence = static_cast<uint16_t>(metadata[2]) |
                             (static_cast<uint16_t>(metadata[3]) << 8);
            payloadLength = static_cast<uint16_t>(metadata[4]) |
                            (static_cast<uint16_t>(metadata[5]) << 8);
            if (metadata[0] != PROTOCOL_VERSION) {
                sendNack(packetSequence, packetCommand, ErrorCode::BadVersion);
                ++errorCount;
                resetReceiver();
            } else if (payloadLength > MAX_PAYLOAD) {
                sendNack(packetSequence, packetCommand, ErrorCode::PayloadTooLarge);
                ++errorCount;
                resetReceiver();
            } else {
                payloadIndex = 0;
                crcIndex = 0;
                receiverState = payloadLength ? ReceiverState::Payload : ReceiverState::Crc;
            }
            return;
        case ReceiverState::Payload:
            packetPayload[payloadIndex++] = value;
            if (payloadIndex == payloadLength) {
                crcIndex = 0;
                receiverState = ReceiverState::Crc;
            }
            return;
        case ReceiverState::Crc:
            crcBytes[crcIndex++] = value;
            if (crcIndex != sizeof(crcBytes)) return;
            {
                const uint16_t received = static_cast<uint16_t>(crcBytes[0]) |
                                          (static_cast<uint16_t>(crcBytes[1]) << 8);
                const uint16_t expected = packetCrc(metadata, packetPayload, payloadLength);
                if (received == expected) {
                    handlePacket(packetCommand, packetSequence, packetPayload, payloadLength);
                } else {
                    sendNack(packetSequence, packetCommand, ErrorCode::BadCrc);
                    ++errorCount;
                }
            }
            resetReceiver();
            return;
    }
}

void pollSerial() {
    size_t budget = MAX_PAYLOAD + 16;
    while (budget) {
        const int available = Serial.available();
        if (available <= 0) break;
        const size_t count = min(
            min(static_cast<size_t>(available), budget),
            sizeof(serialReadBuffer)
        );
        const size_t received = Serial.read(serialReadBuffer, count);
        if (!received) break;
        lastPacketByteAt = millis();
        budget -= received;
        for (size_t index = 0; index < received; ++index) {
            consumeSerialByte(serialReadBuffer[index]);
        }
    }
    if (receiverState != ReceiverState::Magic &&
        millis() - lastPacketByteAt > PACKET_TIMEOUT_MS) {
        ++errorCount;
        resetReceiver();
    }
}

bool installMainFirmware() {
    File firmware = SD.open(MAIN_FIRMWARE, FILE_READ);
    if (!firmware || firmware.isDirectory() || !firmware.size()) {
        if (firmware) firmware.close();
        drawWaitingScreen("Missing main.bin");
        return false;
    }
    const size_t size = firmware.size();
    if (!Update.begin(size, U_FLASH)) {
        firmware.close();
        drawWaitingScreen("Update begin failed");
        return false;
    }

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(52, 15, "Returning to calculator");
    display.drawFrame(4, 28, 248, 9);
    display.sendBuffer();

    uint8_t buffer[1024];
    size_t written = 0;
    uint8_t previousPercent = 255;
    while (firmware.available()) {
        const size_t count = firmware.read(buffer, sizeof(buffer));
        if (!count || Update.write(buffer, count) != count) break;
        written += count;
        const uint8_t percent = static_cast<uint8_t>(written * 100ULL / size);
        if (percent != previousPercent) {
            previousPercent = percent;
            display.setDrawColor(0);
            display.drawBox(5, 29, 246, 7);
            display.setDrawColor(1);
            display.drawBox(5, 29, percent * 246 / 100, 7);
            display.sendBuffer();
        }
        yield();
    }
    firmware.close();
    if (written == size && Update.end(true) && Update.isFinished()) {
        delay(200);
        ESP.restart();
        return true;
    }
    Update.abort();
    drawWaitingScreen("Update failed");
    return false;
}
}

void setup() {
    Serial.setRxBufferSize(SERIAL_RX_BUFFER);
    Serial.begin(SERIAL_BAUD);
    pinMode(OLED_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(OLED_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(18, 19, 23);
    display.begin();
    display.setContrast(220);
    escapeKey.begin();
    sdReady = SD.begin(SD_CS, SPI);
    drawWaitingScreen("Waiting for host");
}

void loop() {
    pollSerial();
    escapeKey.update();

    if (escapeKey.isDown() && escapeKey.heldMs() >= EXIT_HOLD_MS) {
        if (!exitTriggered) {
            exitTriggered = true;
            installMainFirmware();
        }
    } else if (!escapeKey.isDown()) {
        exitTriggered = false;
    }

    if (returnAt && static_cast<int32_t>(millis() - returnAt) >= 0) {
        returnAt = 0;
        installMainFirmware();
    }
    delay(1);
}
