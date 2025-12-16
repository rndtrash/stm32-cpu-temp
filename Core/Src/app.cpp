#include <atomic>
#include "stm32f0xx_ll_usart.h"

#include <app.h>
#include <queue.hpp>

static auto queue = ByteQueue<32>();
bool error = false;

[[noreturn]]
void mainLoop() {
    for (;;) {
        if (auto byte = queue.pop()) {
            // LL_USART_TransmitData8(USART2, 'B');
            LL_USART_TransmitData8(USART2, static_cast<uint8_t>(byte.value()));
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
