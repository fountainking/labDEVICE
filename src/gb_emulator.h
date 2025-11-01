#ifndef GB_EMULATOR_H
#define GB_EMULATOR_H

#include <Arduino.h>
#include <M5Cardputer.h>

// Game Boy emulator using Peanut-GB core
// Single-header Game Boy emulator for ESP32-S3

// State management
extern bool gbActive;

// GB Functions
void enterGB();
void exitGB();
void updateGB();
void drawGB();

// ROM selection
bool selectGBROM();

#endif
