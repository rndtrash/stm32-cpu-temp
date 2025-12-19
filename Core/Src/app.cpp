#include "stm32f0xx_ll_usart.h"

#include <app.h>
#include <packet.hpp>
#include <queue.hpp>

static auto queue = ByteQueue<32>();
static bool error = false;

extern CRC_HandleTypeDef hcrc;

enum class PacketParseState {
    INITIAL = 0,
    FIRST_MAGIC,
    SECOND_MAGIC,
    PACKET_TYPE,
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
    union {
        std::array<std::byte, 16> packetBuffer;
        std::array<uint32_t, 4> packetBuffer32 = {}; // Для выравнивания по границе двойного слова
    };

    const std::optional<size_t> size = packet.serialize(packetBuffer.data(), packetBuffer.size());
    if (!size) return false;

    return crcAndTransmit(packetBuffer32.data(), packetBuffer.size(), size.value());
}

static void parsePacket(const std::byte byte) {
    static auto state = PacketParseState::INITIAL;
    static std::array<std::byte, 16> packetBuffer = {};
    static size_t bufIndex = 0;

    packetBuffer[bufIndex] = byte;

    const auto byteN = static_cast<uint8_t>(byte);
    bool parsingError = false;
    switch (state) {
        case PacketParseState::INITIAL:
            if (byteN == 0xAA) state = PacketParseState::FIRST_MAGIC;
            parsingError = true;
            break;

        case PacketParseState::FIRST_MAGIC:
            if (byteN == 0x55) state = PacketParseState::SECOND_MAGIC;
            parsingError = true;
            break;

        case PacketParseState::SECOND_MAGIC:
            if (byteN >= UART_PACKETS_COUNT) {
                parsingError = true;
                break;
            }

            switch (static_cast<UartPacketTypes>(byte)) {
                case UartPacketTypes::INVALID:
                default:
                    parsingError = true;
                    break;
            }
            break;

        default:
            break;
    }

    if (parsingError) {
        raiseError();
    }
}

[[noreturn]]
void mainLoop() {
    for (;;) {
        if (auto byte = queue.pop()) {
            parsePacket(byte.value());
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
