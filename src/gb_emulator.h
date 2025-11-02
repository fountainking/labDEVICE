#ifndef GB_EMULATOR_H
#define GB_EMULATOR_H

#include <Arduino.h>
#include <M5Cardputer.h>

// Game Boy emulator using Peanut-GB core
// Single-header Game Boy emulator for ESP32-S3
// Set ENABLE_GB_EMULATOR=1 in platformio.ini to enable

#ifdef ENABLE_GB_EMULATOR

// State management
extern bool gbActive;

// GB Functions
void enterGB();
void exitGB();
void updateGB();
void drawGB();

// ROM selection
bool selectGBROM();

#else
// Stubs when GB is disabled
#define gbActive false
inline void enterGB() {}
inline void exitGB() {}
inline void updateGB() {}
inline void drawGB() {}
inline bool selectGBROM() { return false; }
#endif

#endif
