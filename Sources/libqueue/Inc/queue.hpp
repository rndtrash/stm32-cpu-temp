#ifndef STM32_CPU_TEMP_QUEUE_H
#define STM32_CPU_TEMP_QUEUE_H

#include <array>
#include <optional>
#include <cstddef>
#include <atomic>

template<size_t size>
class ByteQueue {
public:
    std::optional<std::byte> pop() {
        if (isEmpty()) return std::nullopt;

        const auto value = array[indexStart];
        indexStart = (indexStart + 1) % size;
        empty = indexStart == indexEnd;

        return value;
    }

    bool push(std::byte byte) {
        if (isFull()) return false;

        array[indexEnd] = byte;
        indexEnd = (indexEnd + 1) % size;
        empty = false;

        return true;
    }

    [[nodiscard]] bool isEmpty() const { return empty; }
    [[nodiscard]] bool isFull() const { return !empty && indexStart == indexEnd; }

private:
    std::array<std::byte, size> array = {};
    std::atomic<size_t> indexStart = 0;
    std::atomic<size_t> indexEnd = 0;
    std::atomic<bool> empty = true;
};

#endif //STM32_CPU_TEMP_QUEUE_H
