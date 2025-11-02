#ifndef EMOJI_MAKER_H
#define EMOJI_MAKER_H

#include <M5Cardputer.h>

// Emoji Maker - Pixel art editor for creating custom emojis
// 16x16 grid, color palette, save to SD or commit to system

// State management
extern bool emojiMakerActive;

// Main functions
void enterEmojiMaker();
void exitEmojiMaker();
void updateEmojiMaker();
void drawEmojiMaker();

// Input handling
void handleEmojiMakerInput();

#endif
