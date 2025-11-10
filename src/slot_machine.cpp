#include "slot_machine.h"
#include "ui.h"
#include "settings.h"
#include <SD.h>
#include <vector>
#include <time.h>

// Global state
SlotMachineState slotState;
bool slotMachineActive = false;

// Physics constants
const float SPIN_START_SPEED = 30.0f;  // Starting speed
const float SPIN_DECELERATION = 0.3f;  // Normal deceleration
const float NEAR_MISS_DECELERATION = 0.1f;  // Slower deceleration for tension
const float MIN_SPEED = 0.5f;  // Stop threshold

// Payout table (multipliers based on bet)
const int PAYOUT_TWO_MATCH = 2;   // 2x bet
const int PAYOUT_THREE_MATCH = 10; // 10x bet
const int PAYOUT_JACKPOT = 50;    // 50x bet (if all 3 match the first emoji in the set)

// Starting coins and daily bonus
const int STARTING_COINS = 100;
const int DAILY_BONUS = 50;

// ============================================================================
// INITIALIZATION
// ============================================================================

void enterSlotMachine() {
  Serial.println("[SLOT] Entering slot machine...");
  slotMachineActive = true;

  Serial.println("[SLOT] Loading coins...");
  // Load coins from SD
  loadCoins();

  Serial.println("[SLOT] Checking daily bonus...");
  // Grant daily bonus if eligible
  if (grantDailyBonus()) {
    Serial.println("[SLOT] Granting daily bonus...");
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.drawString("DAILY BONUS!", 45, 50);
    M5Cardputer.Display.setTextSize(3);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.drawString("+" + String(DAILY_BONUS), 80, 75);
    if (settings.soundEnabled) {
      M5Cardputer.Speaker.tone(1000, 100);
      delay(150);
      M5Cardputer.Speaker.tone(1200, 100);
      delay(150);
      M5Cardputer.Speaker.tone(1500, 200);
    }
    delay(2000);
  }

  Serial.println("[SLOT] Loading emojis...");
  // Load random emojis from SD
  loadRandomEmojis();

  Serial.print("[SLOT] Loaded ");
  Serial.print(slotState.numLoadedEmojis);
  Serial.println(" emojis");

  // Check if emojis loaded
  if (slotState.numLoadedEmojis == 0) {
    Serial.println("[SLOT] ERROR: No emojis found!");
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.drawString("ERROR!", 80, 40);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.drawString("No emojis found in", 50, 70);
    M5Cardputer.Display.drawString("/labchat/emojis/", 50, 85);
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    M5Cardputer.Display.drawString("Press ` to exit", 65, 110);
    slotMachineActive = false;
    return;
  }

  // Initialize game state
  slotState.betAmount = 10;  // Fixed bet of 10 coins
  slotState.spinning = false;
  slotState.showingResults = false;
  slotState.flashScreen = false;
  slotState.pulseWinners = false;
  slotState.matchedReels = 0;
  slotState.winAmount = 0;

  for (int i = 0; i < NUM_REELS; i++) {
    slotState.reelStates[i] = REEL_IDLE;
    slotState.reelPositions[i] = 0.0f;
    slotState.reelSpeeds[i] = 0.0f;
    slotState.reelTargets[i] = random(slotState.numLoadedEmojis);
  }

  Serial.println("[SLOT] Drawing initial screen...");
  // Draw initial screen
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  drawSlotMachine();
  Serial.println("[SLOT] Enter complete!");
}

void exitSlotMachine() {
  slotMachineActive = false;
  saveCoins();

  // Return to games menu
  extern int currentScreenNumber;
  extern void drawGamesMenu();
  currentScreenNumber = 8;
  drawGamesMenu();
}

// ============================================================================
// EMOJI LOADING
// ============================================================================

void loadRandomEmojis() {
  slotState.numLoadedEmojis = 0;

  Serial.println("[SLOT] Checking emoji directory...");

  // Check if emoji directory exists
  if (!SD.exists("/labchat/emojis")) {
    Serial.println("[SLOT] No emoji directory found!");
    return;
  }

  Serial.println("[SLOT] Opening directory...");
  File dir = SD.open("/labchat/emojis");
  if (!dir || !dir.isDirectory()) {
    Serial.println("[SLOT] Failed to open emoji directory!");
    return;
  }

  Serial.println("[SLOT] Scanning for emoji files...");
  // Count and load emojis (no vector - just direct loading)
  File file = dir.openNextFile();
  int filesScanned = 0;

  while (file && slotState.numLoadedEmojis < MAX_LOADED_EMOJIS) {
    filesScanned++;
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.endsWith(".emoji")) {
        String fullPath = "/labchat/emojis/" + name;
        Serial.print("[SLOT] Loading: ");
        Serial.println(name);

        if (loadEmojiFromSD(fullPath, slotState.loadedEmojis[slotState.numLoadedEmojis])) {
          slotState.numLoadedEmojis++;
        }
      }
    }
    file.close();
    file = dir.openNextFile();

    // Safety: don't scan forever
    if (filesScanned > 100) {
      Serial.println("[SLOT] Too many files, stopping scan");
      break;
    }
  }

  dir.close();

  Serial.print("[SLOT] Loaded ");
  Serial.print(slotState.numLoadedEmojis);
  Serial.println(" emojis for slot machine");
}

bool loadEmojiFromSD(const String& path, uint16_t emoji[EMOJI_SIZE][EMOJI_SIZE]) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  // Read raw binary data (512 bytes = 16x16 uint16_t)
  size_t bytesRead = file.read((uint8_t*)emoji, sizeof(uint16_t) * EMOJI_SIZE * EMOJI_SIZE);
  file.close();

  return (bytesRead == sizeof(uint16_t) * EMOJI_SIZE * EMOJI_SIZE);
}

// ============================================================================
// COIN SYSTEM
// ============================================================================

void loadCoins() {
  // Create directory if it doesn't exist
  if (!SD.exists("/slotmachine")) {
    SD.mkdir("/slotmachine");
  }

  File file = SD.open("/slotmachine/coins.dat", FILE_READ);
  if (file) {
    // Read coins, bet amount, and last bonus day
    file.read((uint8_t*)&slotState.coins, sizeof(int));
    file.read((uint8_t*)&slotState.betAmount, sizeof(int));
    file.read((uint8_t*)&slotState.lastDailyBonusDay, sizeof(int));
    file.close();

    // Clamp values
    if (slotState.coins < 0) slotState.coins = STARTING_COINS;
    if (slotState.betAmount < 1) slotState.betAmount = 1;
    if (slotState.betAmount > 10) slotState.betAmount = 10;
  } else {
    // First time - start with 100 coins
    slotState.coins = STARTING_COINS;
    slotState.betAmount = 1;
    slotState.lastDailyBonusDay = -1;
    saveCoins();
  }
}

void saveCoins() {
  if (!SD.exists("/slotmachine")) {
    SD.mkdir("/slotmachine");
  }

  File file = SD.open("/slotmachine/coins.dat", FILE_WRITE);
  if (file) {
    file.write((uint8_t*)&slotState.coins, sizeof(int));
    file.write((uint8_t*)&slotState.betAmount, sizeof(int));
    file.write((uint8_t*)&slotState.lastDailyBonusDay, sizeof(int));
    file.close();
  }
}

bool grantDailyBonus() {
  // Get current day from system time (Unix timestamp / 86400 = days since epoch)
  struct tm timeinfo;
  int currentDay = -1;

  if (getLocalTime(&timeinfo, 100)) {
    // Calculate day of year: month * 31 + day (rough approximation)
    currentDay = timeinfo.tm_mon * 31 + timeinfo.tm_mday;
  } else {
    // No time sync - use millis() based approach (days since boot)
    currentDay = millis() / (1000 * 60 * 60 * 24);
  }

  if (currentDay != slotState.lastDailyBonusDay && currentDay != -1) {
    slotState.lastDailyBonusDay = currentDay;
    slotState.coins += DAILY_BONUS;
    saveCoins();
    return true;
  }

  return false;
}

// ============================================================================
// GAME LOGIC
// ============================================================================

void startSpin() {
  if (slotState.spinning) return;
  if (slotState.coins < slotState.betAmount) return;

  // Deduct bet
  slotState.coins -= slotState.betAmount;

  // Start all reels spinning
  slotState.spinning = true;
  slotState.showingResults = false;
  slotState.matchedReels = 0;
  slotState.winAmount = 0;
  slotState.spinStartTime = millis();  // Track spin start time

  for (int i = 0; i < NUM_REELS; i++) {
    slotState.reelStates[i] = REEL_SPINNING;
    slotState.reelSpeeds[i] = SPIN_START_SPEED;
    slotState.reelTargets[i] = random(slotState.numLoadedEmojis);
  }

  // Play spin sound
  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(800, 50);
  }
}

void updateReels() {
  if (!slotState.spinning) return;

  bool allStopped = true;

  for (int i = 0; i < NUM_REELS; i++) {
    if (slotState.reelStates[i] == REEL_IDLE || slotState.reelStates[i] == REEL_STOPPED) {
      continue;
    }

    allStopped = false;

    // Update position
    slotState.reelPositions[i] += slotState.reelSpeeds[i];

    // Wrap around
    if (slotState.reelPositions[i] >= slotState.numLoadedEmojis) {
      slotState.reelPositions[i] -= slotState.numLoadedEmojis;
    }

    // Apply deceleration
    if (slotState.reelStates[i] == REEL_SPINNING) {
      // Check if it's time to start slowing down (stagger reel stops)
      // Reel 0 stops after 0.8s, reel 1 after 1.2s, reel 2 after 1.6s
      unsigned long elapsed = millis() - slotState.spinStartTime;
      unsigned long stopTime = 800 + (i * 400);  // Stagger stops

      if (elapsed > stopTime) {
        slotState.reelStates[i] = REEL_SLOWING;

        // Check for near-miss (2 reels match, slow down 3rd)
        if (i == 2) {  // Last reel
          // Check if first two reels match
          int reel0Target = slotState.reelTargets[0];
          int reel1Target = slotState.reelTargets[1];

          if (reel0Target == reel1Target) {
            // NEAR MISS TENSION!
            slotState.reelSpeeds[i] *= 0.4f;  // Slow down dramatically
          }
        }
      }
    } else if (slotState.reelStates[i] == REEL_SLOWING) {
      // Apply deceleration
      float decel = SPIN_DECELERATION;

      // Near-miss: slower deceleration for last reel if first 2 match
      if (i == 2) {
        int reel0Target = slotState.reelTargets[0];
        int reel1Target = slotState.reelTargets[1];
        if (reel0Target == reel1Target) {
          decel = NEAR_MISS_DECELERATION;
        }
      }

      slotState.reelSpeeds[i] -= decel;

      // Stop if speed is too low
      if (slotState.reelSpeeds[i] <= MIN_SPEED) {
        stopReel(i);

        // Play tone when reel stops (ascending pitches)
        if (settings.soundEnabled) {
          int pitch = 1000 + (i * 200);
          M5Cardputer.Speaker.tone(pitch, 100);
        }
      }
    }

    // Play clicking sounds during spin
    static unsigned long lastClickTime = 0;
    if (slotState.reelStates[i] == REEL_SPINNING && millis() - lastClickTime > 50) {
      playClickSound();
      lastClickTime = millis();
    }
  }

  if (allStopped) {
    slotState.spinning = false;
    checkWin();
  }
}

void stopReel(int reelIndex) {
  slotState.reelStates[reelIndex] = REEL_STOPPED;
  slotState.reelSpeeds[reelIndex] = 0.0f;

  // Snap to target position
  slotState.reelPositions[reelIndex] = (float)slotState.reelTargets[reelIndex];
}

void checkWin() {
  // Get final emoji indices
  int emoji0 = slotState.reelTargets[0];
  int emoji1 = slotState.reelTargets[1];
  int emoji2 = slotState.reelTargets[2];

  // Check for matches
  if (emoji0 == emoji1 && emoji1 == emoji2) {
    // THREE MATCH!
    slotState.matchedReels = 3;

    // Check for jackpot (all match first emoji)
    if (emoji0 == 0) {
      slotState.winAmount = slotState.betAmount * PAYOUT_JACKPOT;
      playJackpotSound();
    } else {
      slotState.winAmount = slotState.betAmount * PAYOUT_THREE_MATCH;
      playWinSound(3);
    }

    triggerFlash();
    triggerPulse();
  } else if (emoji0 == emoji1 || emoji1 == emoji2 || emoji0 == emoji2) {
    // TWO MATCH
    slotState.matchedReels = 2;
    slotState.winAmount = slotState.betAmount * PAYOUT_TWO_MATCH;
    playWinSound(2);
    triggerPulse();
  } else {
    // NO MATCH
    slotState.matchedReels = 0;
    slotState.winAmount = 0;

    // Play sad sound
    if (settings.soundEnabled) {
      M5Cardputer.Speaker.tone(400, 100);
      delay(120);
      M5Cardputer.Speaker.tone(300, 150);
    }
  }

  // Pay out winnings
  if (slotState.winAmount > 0) {
    slotState.coins += slotState.winAmount;
    saveCoins();
  }

  slotState.showingResults = true;
}

// ============================================================================
// DRAWING
// ============================================================================

void drawSlotMachine() {
  Serial.println("[SLOT] Drawing...");

  M5Cardputer.Display.fillScreen(TFT_BLACK);

  // Status bar
  Serial.println("[SLOT] Status bar...");
  drawStatusBar(false);

  // Draw reels (centered, bigger)
  Serial.println("[SLOT] Reels...");
  drawReels();

  // Draw UI
  Serial.println("[SLOT] UI...");
  drawUI();

  // Apply flash effect if active
  if (slotState.flashScreen) {
    updateFlash();
  }

  // Apply pulse effect if active
  if (slotState.pulseWinners) {
    updatePulse();
  }

  Serial.println("[SLOT] Draw complete!");
}

void drawReels() {
  // Calculate reel display area (center of screen, bigger emojis)
  int emojiDisplaySize = 32;  // 2x scale (16x16 -> 32x32)
  int reelWidth = 36;  // Wider for bigger emojis
  int reelSpacing = 12;
  int startX = (240 - (NUM_REELS * reelWidth + (NUM_REELS - 1) * reelSpacing)) / 2;
  int startY = 40;  // Centered vertically

  for (int r = 0; r < NUM_REELS; r++) {
    int reelX = startX + r * (reelWidth + reelSpacing);

    // Draw reel background
    M5Cardputer.Display.fillRect(reelX, startY, reelWidth, 70, TFT_DARKGREY);
    M5Cardputer.Display.drawRect(reelX, startY, reelWidth, 70, TFT_WHITE);

    // Draw emoji on this reel
    int currentEmojiIndex = (int)slotState.reelPositions[r];

    // Center position for emoji
    int emojiX = reelX + 2;  // Small margin
    int emojiY = startY + 19;  // Centered vertically in reel

    // Draw the current emoji (2x scale: 16x16 -> 32x32)
    if (currentEmojiIndex < slotState.numLoadedEmojis) {
      // 2x scaling: each pixel becomes 2x2
      for (int py = 0; py < EMOJI_SIZE; py++) {
        for (int px = 0; px < EMOJI_SIZE; px++) {
          uint16_t color = slotState.loadedEmojis[currentEmojiIndex][py][px];
          // Draw 2x2 block for each original pixel
          M5Cardputer.Display.fillRect(emojiX + (px * 2), emojiY + (py * 2), 2, 2, color);
        }
      }
    }

    // Motion blur effect during spin (vertical lines)
    if (slotState.reelStates[r] == REEL_SPINNING) {
      for (int i = 0; i < 5; i++) {
        int lineY = startY + random(70);
        M5Cardputer.Display.drawFastHLine(reelX, lineY, reelWidth, TFT_LIGHTGREY);
      }
    }

    // Highlight winning reel
    if (slotState.showingResults && slotState.matchedReels > 0) {
      bool isWinningReel = false;

      if (slotState.matchedReels == 3) {
        isWinningReel = true;  // All reels win
      } else if (slotState.matchedReels == 2) {
        // Check which reels match
        int e0 = slotState.reelTargets[0];
        int e1 = slotState.reelTargets[1];
        int e2 = slotState.reelTargets[2];

        if ((r == 0 || r == 1) && e0 == e1) isWinningReel = true;
        if ((r == 1 || r == 2) && e1 == e2) isWinningReel = true;
        if ((r == 0 || r == 2) && e0 == e2) isWinningReel = true;
      }

      if (isWinningReel) {
        M5Cardputer.Display.drawRect(reelX - 1, startY - 1, reelWidth + 2, 72, TFT_YELLOW);
        M5Cardputer.Display.drawRect(reelX - 2, startY - 2, reelWidth + 4, 74, TFT_YELLOW);
      }
    }
  }
}

void drawUI() {
  // Coin display removed - logic kept for future use

  // Result display (center bottom)
  if (slotState.showingResults && slotState.winAmount > 0) {
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    String winText = "WIN +" + String(slotState.winAmount);
    int textWidth = winText.length() * 12;
    M5Cardputer.Display.drawString(winText.c_str(), (240 - textWidth) / 2, 115);
  } else if (slotState.showingResults) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.drawString("No match", 90, 120);
  }

  // Controls hint
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.drawString("Enter=Spin  `=Exit", 70, 125);
}

void drawCoinDisplay() {
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.drawString("Coins:", 180, 10);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString(String(slotState.coins).c_str(), 185, 20);
}

void drawBetControls() {
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.drawString("Bet:", 10, 100);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString(String(slotState.betAmount).c_str(), 40, 100);

  // Bet amount bar
  int barWidth = slotState.betAmount * 10;
  M5Cardputer.Display.fillRect(10, 110, barWidth, 5, TFT_GREEN);
  M5Cardputer.Display.drawRect(10, 110, 100, 5, TFT_DARKGREY);
}

// ============================================================================
// VISUAL EFFECTS
// ============================================================================

void triggerFlash() {
  slotState.flashScreen = true;
  slotState.flashStartTime = millis();
}

void updateFlash() {
  unsigned long elapsed = millis() - slotState.flashStartTime;

  if (elapsed < 100) {
    // Invert screen (simple flash)
    M5Cardputer.Display.invertDisplay(true);
  } else if (elapsed < 200) {
    M5Cardputer.Display.invertDisplay(false);
  } else if (elapsed < 300) {
    M5Cardputer.Display.invertDisplay(true);
  } else {
    M5Cardputer.Display.invertDisplay(false);
    slotState.flashScreen = false;
  }
}

void triggerPulse() {
  slotState.pulseWinners = true;
  slotState.pulseStartTime = millis();
}

void updatePulse() {
  unsigned long elapsed = millis() - slotState.pulseStartTime;

  if (elapsed > 1000) {
    slotState.pulseWinners = false;
    return;
  }

  // Pulse effect already handled in drawReels() with yellow highlight
}

// ============================================================================
// AUDIO
// ============================================================================

void playClickSound() {
  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(600, 20);
  }
}

void playWinSound(int matchCount) {
  if (!settings.soundEnabled) return;

  if (matchCount == 2) {
    // Two match - simple ascending tones
    M5Cardputer.Speaker.tone(800, 100);
    delay(120);
    M5Cardputer.Speaker.tone(1000, 150);
  } else if (matchCount == 3) {
    // Three match - fanfare
    M5Cardputer.Speaker.tone(1000, 100);
    delay(120);
    M5Cardputer.Speaker.tone(1200, 100);
    delay(120);
    M5Cardputer.Speaker.tone(1500, 200);
  }
}

void playJackpotSound() {
  if (!settings.soundEnabled) return;

  // Epic jackpot sound
  for (int i = 0; i < 3; i++) {
    M5Cardputer.Speaker.tone(800 + (i * 200), 100);
    delay(120);
  }
  M5Cardputer.Speaker.tone(2000, 300);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void handleSlotMachineInput() {
  if (!slotMachineActive) return;

  Serial.println("[SLOT] Input handler called");

  // DON'T call update() - main loop already does it
  // DON'T check isChange() - main keyboard handler already did
  auto status = M5Cardputer.Keyboard.keysState();

  Serial.println("[SLOT] Checking keys...");

  // ESC to exit
  for (auto key : status.word) {
    Serial.print("[SLOT] Key: ");
    Serial.println(key);

    if (key == '`') {
      Serial.println("[SLOT] Exit key pressed!");
      exitSlotMachine();
      return;
    }
  }

  // ENTER to spin
  if (status.enter && !slotState.spinning) {
    Serial.println("[SLOT] ENTER pressed - starting spin!");
    startSpin();
  }
}

// ============================================================================
// MAIN UPDATE LOOP
// ============================================================================

void updateSlotMachine() {
  if (!slotMachineActive) return;

  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    Serial.println("[SLOT] Update loop running...");
    lastDebug = millis();
  }

  // Only update during spin
  if (slotState.spinning) {
    static unsigned long lastUpdate = 0;
    // Throttle updates to ~60fps during spin
    if (millis() - lastUpdate > 16) {
      Serial.println("[SLOT] Updating reels...");
      updateReels();
      // Only redraw the reels, not the whole screen!
      drawReels();
      lastUpdate = millis();
    }
  }

  // No delay here - main loop handles timing
}
