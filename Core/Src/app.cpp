#include "stm32f0xx_ll_usart.h"

#include <app.h>
#include <packet.hpp>
#include <queue.hpp>

static auto queue = ByteQueue<32>();
static bool error = false;

extern CRC_HandleTypeDef hcrc;

enum class PacketParseState {
    FIRST_MAGIC = 0,
    SECOND_MAGIC,
    PACKET_TYPE,
    PAYLOAD,
    CRC32
};

static bool crcAndTransmit(uint32_t *buffer, const size_t bufferSize, const size_t packetSize) {
    const auto crc = HAL_CRC_Calculate(&hcrc, buffer, packetSize);
    if (bufferSize < packetSize + sizeof(crc)) return false;

    auto *byteBuffer = reinterpret_cast<uint8_t *>(buffer);
    for (size_t i = 0; i < sizeof(crc); i++) {
        byteBuffer[packetSize + i] = static_cast<uint8_t>((crc & (0xFF << i * 8)) >> (i * 8));
    }

    const auto sizeFinal = packetSize + sizeof(crc);
    for (size_t i = 0; i < sizeFinal; i++) {
        LL_USART_TransmitData8(USART2, byteBuffer[i]);

        const auto start = HAL_GetTick();
        constexpr auto maxWaitMs = 100;
        while (!LL_USART_IsActiveFlag_TXE(USART2) && HAL_GetTick() - start < maxWaitMs) {
        }

        if (HAL_GetTick() - start >= maxWaitMs)
            return false;
    }

    return true;
}

template<typename T>
[[nodiscard]]
static bool sendPacket(T &packet) {
    alignas(uint32_t) std::array<std::byte, 16> packetBuffer = {};

    const std::optional<size_t> size = packet.serialize(packetBuffer.data(), packetBuffer.size());
    if (!size) return false;

    return crcAndTransmit(reinterpret_cast<uint32_t *>(packetBuffer.data()), packetBuffer.size(), size.value());
}

static void sendTemperature() {
    // TODO: чтение настоящей температуры
    const auto packet = UartPacketResponseTemp(13.37f);
    if (!sendPacket(packet)) {
        raiseError();
    }
}

static void sendPong() {
    if (const auto packet = UartPacketPong(); !sendPacket(packet)) {
        raiseError();
    }
}

namespace Parser {
    static auto state = PacketParseState::FIRST_MAGIC;
    static size_t bufIndex = 0;
    static size_t bufIndexCrc = 0;

    static void reset() {
        state = PacketParseState::FIRST_MAGIC;
        bufIndex = 0;
    }

    static void startCRC32() {
        state = PacketParseState::CRC32;
        bufIndexCrc = bufIndex;
    }

    static void parsePacket(const std::byte byte) {
        alignas(uint32_t) static std::array<std::byte, 16> packetBuffer = {};
        static auto packetType = UartPacketTypes::INVALID;

        if (bufIndex >= packetBuffer.size()) {
            raiseError();
            return;
        }
        packetBuffer[bufIndex++] = byte;

        const auto byteN = static_cast<uint8_t>(byte);
        bool parsingError = false;
        switch (state) {
            case PacketParseState::FIRST_MAGIC:
                if (byteN == 0xAA) state = PacketParseState::SECOND_MAGIC;
                else parsingError = true;
                break;

            case PacketParseState::SECOND_MAGIC:
                if (byteN == 0x55) state = PacketParseState::PACKET_TYPE;
                else parsingError = true;
                break;

            case PacketParseState::PACKET_TYPE:
                if (byteN >= UART_PACKETS_COUNT || (packetType = static_cast<UartPacketTypes>(byte)) ==
                    UartPacketTypes::INVALID) {
                    parsingError = true;
                    break;
                }

                // TODO: парсинг для пакетов с доп. данными
                if (packetType == UartPacketTypes::RESPONSE) {
                    state = PacketParseState::PAYLOAD;
                } else {
                    startCRC32();
                }
                break;

            case PacketParseState::PAYLOAD:
                // TODO: парсинг для пакетов с доп. данными
                startCRC32();
                break;

            case PacketParseState::CRC32:
                if (bufIndex - bufIndexCrc == 4) {
                    const auto buffer32 = reinterpret_cast<uint32_t *>(packetBuffer.data());
                    const auto crcActual = HAL_CRC_Calculate(&hcrc, buffer32, bufIndexCrc);

                    uint32_t crcReceived = 0;
                    for (size_t i = 0; i < sizeof(crcReceived); i++) {
                        crcReceived |= static_cast<uint8_t>(packetBuffer[bufIndexCrc + i]) << (i * 8);
                    }

                    if (crcReceived == crcActual) {
                        switch (packetType) {
                            case UartPacketTypes::PING:
                                sendPong();
                                break;

                            case UartPacketTypes::REQUEST:
                                sendTemperature();
                                break;

                            // TODO: continuous
                            case UartPacketTypes::REQUEST_CONTINUOUS_START:
                            case UartPacketTypes::REQUEST_CONTINUOUS_STOP:

                            default:
                                parsingError = true;
                                break;
                        }
                    } else {
                        parsingError = true;
                    }
                    reset();
                }
                break;

            default:
                break;
        }

        if (parsingError) {
            raiseError();
            reset();
        }
    }
}

[[noreturn]]
void mainLoop() {
    for (;;) {
        if (auto byte = queue.pop()) {
            Parser::parsePacket(byte.value());
        }

        if (error) {
            error = false;
            const auto errorPacket = UartPacketError();
            // Игнорируем результат, потому что если всё пошло не так - всё равно не сможем сообщить об ошибке
            static_cast<void>(sendPacket(errorPacket));
        }
    }
}

void appEnqueue(uint8_t byte) {
    if (!queue.push(static_cast<std::byte>(byte)))
        error = true;
}

void raiseError() {
    error = true;
}
