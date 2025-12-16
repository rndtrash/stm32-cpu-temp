#ifndef STM32_CPU_TEMP_APP_H
#define STM32_CPU_TEMP_APP_H

#ifdef __cplusplus
extern "C" {
#endif

[[noreturn]] void mainLoop();
void appEnqueue(uint8_t byte);
void raiseError();

#ifdef __cplusplus
}
#endif

#endif //STM32_CPU_TEMP_APP_H