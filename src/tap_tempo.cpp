#include "tap_tempo.h"
#include "ui.h"
#include "settings.h"

// Tap tempo state
unsigned long tapHistory[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // More samples for accuracy
int tapCount = 0;
int currentBPM = 0;
unsigned long lastTapTime = 0;
unsigned long tapTimeout = 2500; // Reset after 2.5 seconds
bool tempoLocked = false;
float avgIntervalMs = 0;  // Running average
unsigned long nextBeatTime = 0;  // For visual metronome
bool beatFlash = false;
bool metronomeSound = false;  // Sound off by default

void enterTapTempo() {
  // Reset state
  for (int i = 0; i < 8; i++) {
    tapHistory[i] = 0;
  }
  tapCount = 0;
  currentBPM = 0;
  lastTapTime = 0;
  tempoLocked = false;
  avgIntervalMs = 0;
  nextBeatTime = 0;
  beatFlash = false;
  metronomeSound = false;  // Start with sound off

  canvas.fillScreen(TFT_BLACK);
  drawTapTempo();
}

void exitTapTempo() {
  // Cleanup
}

void drawTapTempo() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  // Beat flash indicator (left side, Ctenophore-inspired bioluminescent colors)
  int dotX = 30;
  int dotY = 50;

  if (beatFlash) {
    // Ctenophore bioluminescence gradient: Deep blue -> Cyan -> White (like ctempo project)
    static int colorIndex = 0;
    uint16_t colors[] = {
      0x001F,  // Deep blue
      0x051F,  // Light blue
      0x07FF,  // Cyan
      0x87F0,  // Cyan-white
      0xFFFF   // White (peak flash)
    };
    canvas.fillCircle(dotX, dotY, 18, colors[colorIndex % 5]);
    colorIndex++;
  } else {
    canvas.drawCircle(dotX, dotY, 18, TFT_DARKGREY);
  }

  // BPM display (HUGE) - centered, raised up
  canvas.setTextSize(4);
  if (currentBPM > 0) {
    // Ctenophore cyan theme for tempo display
    uint16_t color = tempoLocked ? 0x07FF : 0x87F0;  // Cyan when locked, cyan-white when adjusting
    canvas.setTextColor(color);

    String bpmStr = String(currentBPM);
    int textW = bpmStr.length() * 24;  // Approx width
    int textX = (240 - textW) / 2;
    canvas.drawString(bpmStr, textX, 35);

    canvas.setTextSize(1);
    canvas.setTextColor(0x87F0);  // Cyan-white
    canvas.drawString("BPM", 105, 70);
  } else {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("---", 80, 40);
  }

  // Tap counter with progress bar - raised (purple theme)
  canvas.setTextSize(1);
  canvas.setTextColor(0xA11F);  // Medium purple
  String tapsMsg = "Taps: " + String(tapCount) + "/8";
  canvas.drawString(tapsMsg, 85, 80);

  // Progress bar - raised (purple)
  int barWidth = (tapCount * 200) / 8;
  canvas.drawRect(20, 95, 200, 6, 0x780F);  // Dark purple outline
  if (tapCount > 0) {
    canvas.fillRect(20, 95, barWidth, 6, 0xF81F);  // Bright purple/magenta
  }

  // Status message - raised (purple)
  canvas.setTextColor(0xA11F);  // Medium purple
  if (tapCount == 0) {
    canvas.drawString("Tap SPACE to start", 60, 105);
  } else if (tapCount < 3) {
    canvas.drawString("Keep tapping...", 70, 105);
  } else if (!tempoLocked) {
    canvas.drawString("Locking tempo...", 70, 105);
  } else {
    canvas.setTextColor(0xF81F);  // Bright purple when locked
    canvas.drawString("Locked!", 90, 105);
  }

  // Instructions - raised and not cut off (purple)
  canvas.setTextSize(1);
  canvas.setTextColor(0xA11F);  // Medium purple
  canvas.drawString("SPACE=Tap R=Reset S=Sound `=Back", 20, 118);

  // Sound indicator (purple when on)
  canvas.setTextColor(metronomeSound ? 0xF81F : 0x780F);  // Bright purple / dark purple
  canvas.drawString(metronomeSound ? "SOUND ON" : "SOUND OFF", 165, 25);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handleTapTempoNavigation(char key) {
  if (key == ' ') {
    // Space bar tap
    unsigned long currentTime = millis();

    // Reset if too much time passed
    if (tapCount > 0 && (currentTime - lastTapTime > tapTimeout)) {
      tapCount = 0;
      currentBPM = 0;
      tempoLocked = false;
      avgIntervalMs = 0;
    }

    lastTapTime = currentTime;

    // Store tap time (sliding window of 8 taps)
    if (tapCount < 8) {
      tapHistory[tapCount] = currentTime;
      tapCount++;
    } else {
      // Shift left and add new tap
      for (int i = 0; i < 7; i++) {
        tapHistory[i] = tapHistory[i + 1];
      }
      tapHistory[7] = currentTime;
    }

    // Calculate BPM from tap intervals
    if (tapCount >= 2) {
      unsigned long totalInterval = 0;
      int intervals = 0;

      // Average all intervals (weighted toward recent taps)
      int startIdx = (tapCount > 4) ? tapCount - 4 : 1;  // Use last 4 intervals for accuracy
      for (int i = startIdx; i < tapCount && i < 8; i++) {
        totalInterval += (tapHistory[i] - tapHistory[i-1]);
        intervals++;
      }

      if (intervals > 0) {
        avgIntervalMs = (float)totalInterval / intervals;
        currentBPM = 60000 / avgIntervalMs;

        // Clamp to reasonable range
        if (currentBPM < 30) currentBPM = 30;
        if (currentBPM > 300) currentBPM = 300;

        // Lock after 3 taps (2 intervals)
        tempoLocked = (tapCount >= 3);

        // Always schedule next beat from current tap
        nextBeatTime = currentTime + avgIntervalMs;
      }
    }

    // Visual + audio feedback
    beatFlash = true;
    if (settings.soundEnabled) {
      int pitch = tempoLocked ? 1200 : 1000;
      M5Cardputer.Speaker.tone(pitch, 50);
    }

    drawTapTempo();
  } else if (key == 's' || key == 'S') {
    // Toggle sound
    metronomeSound = !metronomeSound;

    if (settings.soundEnabled) {
      M5Cardputer.Speaker.tone(metronomeSound ? 1200 : 800, 50);
    }

    drawTapTempo();
  } else if (key == 'r' || key == 'R') {
    // Reset
    for (int i = 0; i < 8; i++) {
      tapHistory[i] = 0;
    }
    tapCount = 0;
    currentBPM = 0;
    lastTapTime = 0;
    tempoLocked = false;
    avgIntervalMs = 0;
    nextBeatTime = 0;
    beatFlash = false;

    if (settings.soundEnabled) {
      M5Cardputer.Speaker.tone(400, 100);
    }

    drawTapTempo();
  }
}

void updateTapTempo() {
  unsigned long now = millis();

  // Visual metronome (beat flash) - runs forever once locked
  if (tempoLocked && avgIntervalMs > 0) {
    if (now >= nextBeatTime) {
      beatFlash = true;

      // Audio click if sound enabled
      if (metronomeSound && settings.soundEnabled) {
        M5Cardputer.Speaker.tone(1500, 30);
      }

      // Schedule next beat
      nextBeatTime = now + avgIntervalMs;

      drawTapTempo();
      delay(50);

      beatFlash = false;
      drawTapTempo();
    }
  }

  // Turn off beat flash after brief delay
  if (beatFlash && (now - lastTapTime > 50)) {
    beatFlash = false;
    drawTapTempo();
  }
}
