#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HEX 16
inline uint32_t millis(){return 0;}
inline uint32_t micros(){return 0;}
inline void delay(uint32_t){}
inline void delayMicroseconds(uint32_t){}
inline void pinMode(uint8_t,int){}
inline void digitalWrite(uint8_t,int){}
inline void noInterrupts(){}
inline void interrupts(){}
inline void yield(){}
struct RP2040Stub { int getFreeStack(){return 8192;} };
extern RP2040Stub rp2040;
