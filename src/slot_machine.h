#ifndef SLOT_MACHINE_H
#define SLOT_MACHINE_H

#include <Arduino.h>
#include <M5Cardputer.h>

// Slot machine constants
#define MAX_LOADED_EMOJIS 12      // Max emojis to keep in RAM
#define EMOJI_SIZE 16             // 16x16 pixels
#define NUM_REELS 3               // 3 slot reels
#define REEL_HEIGHT 5             // Show 5 emojis per reel (scrolling)

// Reel states
enum ReelState {
  REEL_IDLE,
  REEL_SPINNING,
  REEL_SLOWING,
  REEL_STOPPED
};

// Slot machine game state
struct SlotMachineState {
  int coins;
  int betAmount;
  int lastDailyBonusDay;  // Day of year for daily bonus tracking

  // Loaded emojis (16x16 RGB565)
  uint16_t loadedEmojis[MAX_LOADED_EMOJIS][EMOJI_SIZE][EMOJI_SIZE];
  int numLoadedEmojis;

  // Reel state
  ReelState reelStates[NUM_REELS];
  float reelPositions[NUM_REELS];  // Floating point for smooth animation
  float reelSpeeds[NUM_REELS];     // Current speed of each reel
  int reelTargets[NUM_REELS];      // Target emoji index for each reel

  // Visual effects
  bool flashScreen;
  unsigned long flashStartTime;
  bool pulseWinners;
  unsigned long pulseStartTime;

  // Game flow
  bool spinning;
  bool showingResults;
  int matchedReels;  // 0, 2, or 3
  int winAmount;
  unsigned long spinStartTime;  // Track when spin started for timing
};

// Global state
extern SlotMachineState slotState;
extern bool slotMachineActive;

// Core functions
void enterSlotMachine();
void exitSlotMachine();
void updateSlotMachine();
void drawSlotMachine();
void handleSlotMachineInput();

// Emoji loading
void loadRandomEmojis();
bool loadEmojiFromSD(const String& path, uint16_t emoji[EMOJI_SIZE][EMOJI_SIZE]);

// Coin system
void loadCoins();
void saveCoins();
bool grantDailyBonus();

// Game logic
void startSpin();
void updateReels();
void stopReel(int reelIndex);
void checkWin();
void payoutWin();

// Drawing
void drawReels();
void drawUI();
void drawCoinDisplay();
void drawBetControls();
void drawPaytable();

// Effects
void triggerFlash();
void updateFlash();
void triggerPulse();
void updatePulse();
void playClickSound();
void playWinSound(int matchCount);
void playJackpotSound();

#endif
