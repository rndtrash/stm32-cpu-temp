#ifndef STM32_CPU_TEMP_PACKET_H
#define STM32_CPU_TEMP_PACKET_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

constexpr uint16_t UART_PACKET_MAGIC = 0x55AA; // В буфере: 0xAA 0x55

enum class UartPacketTypes : uint8_t {
    INVALID = 0,
    ERROR,
    PING,
    PONG,
    REQUEST,
    REQUEST_CONTINUOUS_START,
    REQUEST_CONTINUOUS_STOP,
    RESPONSE,
    TYPES_COUNT
};

constexpr auto UART_PACKETS_COUNT = static_cast<std::underlying_type_t<UartPacketTypes>>(UartPacketTypes::TYPES_COUNT);

struct UartPacket {
    uint16_t magic = UART_PACKET_MAGIC;
    UartPacketTypes type = UartPacketTypes::INVALID;

    std::optional<size_t> serialize(std::byte *buffer, const size_t bufferSize) const {
        constexpr auto packetSize = sizeof(magic) + sizeof(type);
        if (bufferSize < packetSize) return std::nullopt;

        // TODO: std::byteswap из C++23 очень хочется
        buffer[0] = static_cast<std::byte>(magic & 0x00FF);
        buffer[1] = static_cast<std::byte>((magic & 0xFF00) >> 8);
        buffer[2] = static_cast<std::byte>(type);

        return packetSize;
    }
};

struct UartPacketResponseTemp : UartPacket {
    explicit UartPacketResponseTemp(const float temp) : UartPacket(), temperature(temp) { type = UartPacketTypes::RESPONSE; }

    std::optional<size_t> serialize(std::byte *buffer, const size_t bufferSize) const {
        if (const auto packetSizeO = UartPacket::serialize(buffer, bufferSize)) {
            const auto packetSize = packetSizeO.value();
            if (bufferSize < packetSize + sizeof(temperature)) return std::nullopt;

            const auto temperatureBytes = reinterpret_cast<const std::byte *>(&temperature);
            std::memmove(buffer + packetSize, temperatureBytes, sizeof(temperature));

            return packetSize + sizeof(temperature);
        }
        return std::nullopt;
    }

    float temperature = .0f;
};

struct UartPacketPong : UartPacket {
    explicit UartPacketPong() : UartPacket() { type = UartPacketTypes::PONG; }
};

struct UartPacketError : UartPacket {
    explicit UartPacketError() : UartPacket() { type = UartPacketTypes::ERROR; }
    std::optional<size_t> serialize(std::byte *buffer, const size_t bufferSize) const { return UartPacket::serialize(buffer, bufferSize); }
};

#endif //STM32_CPU_TEMP_PACKET_H
