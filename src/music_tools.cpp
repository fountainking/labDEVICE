#include "music_tools.h"
#include "ui.h"
#include "settings.h"
#include "lbm.h"
#include "tap_tempo.h"

MusicToolsState musicToolsState = TOOLS_MENU;
int musicToolsMenuIndex = 0;

// Guitar tuner state
bool tunerActive = false;
float detectedFrequency = 0.0;
int closestString = -1;
float centsDiff = 0.0;
int lastClosestString = -1;
int lastTuningState = 0; // -1=flat, 0=in tune, 1=sharp

// Audio visualizer state
bool visualizerActive = false;
int16_t barHeights[20];  // Heights for equalizer bars
int barIndex = 0;

// Standard guitar tuning (Hz)
struct GuitarString {
  const char* name;
  float frequency;
};

const GuitarString guitarStrings[] = {
  {"E", 82.41},   // Low E
  {"A", 110.00},  // A
  {"D", 146.83},  // D
  {"G", 196.00},  // G
  {"B", 246.94},  // B
  {"E", 329.63}   // High E
};
const int numStrings = 6;

// Audio sampling
#define SAMPLE_RATE 16000
#define BUFFER_SIZE 2048
int16_t audioBuffer[BUFFER_SIZE];
float signalLevel = 0.0;

// Use M5Cardputer's built-in microphone
void initMicrophone() {
  M5Cardputer.Mic.begin();
}

void deinitMicrophone() {
  M5Cardputer.Mic.end();
}

// Improved autocorrelation for frequency detection
float detectFrequency() {
  // Record audio using M5Cardputer's mic
  if (M5Cardputer.Mic.isEnabled()) {
    if (M5Cardputer.Mic.record(audioBuffer, BUFFER_SIZE, SAMPLE_RATE)) {
      // Amplify the signal for better sensitivity (2x gain)
      for (int i = 0; i < BUFFER_SIZE; i++) {
        int32_t amplified = audioBuffer[i] * 2;
        // Clamp to prevent overflow
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        audioBuffer[i] = amplified;
      }

      // Calculate signal level (RMS)
      float sum = 0.0;
      for (int i = 0; i < BUFFER_SIZE; i++) {
        sum += abs(audioBuffer[i]);
      }
      signalLevel = sum / BUFFER_SIZE;

      // Lower noise threshold for better sensitivity (was 500, now 250)
      if (signalLevel < 250) return 0.0;
    } else {
      return 0.0;
    }
  } else {
    return 0.0;
  }

  // Autocorrelation for pitch detection
  int maxLag = SAMPLE_RATE / 60;   // Minimum ~60 Hz (below low E)
  int minLag = SAMPLE_RATE / 400;  // Maximum ~400 Hz (above high E)

  float maxCorr = 0.0;
  int maxLagIndex = 0;

  // Calculate zero-lag correlation for normalization
  float zeroCorr = 0.0;
  for (int i = 0; i < BUFFER_SIZE / 2; i++) {
    zeroCorr += (float)audioBuffer[i] * audioBuffer[i];
  }

  for (int lag = minLag; lag < maxLag && lag < BUFFER_SIZE / 2; lag++) {
    float corr = 0.0;
    for (int i = 0; i < BUFFER_SIZE / 2; i++) {
      corr += (float)audioBuffer[i] * audioBuffer[i + lag];
    }

    // Normalize correlation
    corr = corr / zeroCorr;

    if (corr > maxCorr) {
      maxCorr = corr;
      maxLagIndex = lag;
    }
  }

  // Need strong correlation to be confident
  if (maxCorr < 0.5 || maxLagIndex == 0) return 0.0;

  return (float)SAMPLE_RATE / maxLagIndex;
}

// Find closest guitar string
void analyzeFrequency(float freq) {
  if (freq < 60.0 || freq > 400.0) {
    closestString = -1;
    return;
  }

  float minDiff = 1000.0;
  closestString = -1;

  for (int i = 0; i < numStrings; i++) {
    float diff = abs(freq - guitarStrings[i].frequency);
    if (diff < minDiff) {
      minDiff = diff;
      closestString = i;
    }
  }

  if (closestString >= 0) {
    // Calculate cents difference (100 cents = 1 semitone)
    centsDiff = 1200.0 * log2(freq / guitarStrings[closestString].frequency);
  }
}

void enterMusicTools() {
  musicToolsState = TOOLS_MENU;
  musicToolsMenuIndex = 0;
  drawMusicToolsMenu();
}

void drawMusicToolsMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  // No title - removed

  // Menu items
  const char* menuItems[] = {"Guitar Tuner", "Equalizer", "Lab Beat Machine", "Tap Tempo"};
  const int menuCount = 4;

  canvas.setTextSize(1);
  int yPos = 35;  // Lifted up

  for (int i = 0; i < menuCount; i++) {
    if (i == musicToolsMenuIndex) {
      // Selected item - white highlight
      canvas.fillRoundRect(10, yPos - 2, 220, 16, 3, TFT_WHITE);
      canvas.setTextColor(TFT_WHITE);
    } else {
      // Use different purple shades
      uint16_t itemColor = (i == 0) ? 0xF81F : (i == 1) ? 0xC99F : 0xA11F;  // Bright, light, medium purple
      canvas.setTextColor(itemColor);
    }
    canvas.drawString(menuItems[i], 15, yPos);
    yPos += 20;
  }

  // Instructions
  canvas.setTextColor(0x780F);  // Deep purple
  canvas.drawString("`;/.` Navigate | Enter Select", 20, 115);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void startGuitarTuner() {
  tunerActive = true;
  detectedFrequency = 0.0;
  closestString = -1;
  centsDiff = 0.0;
  lastClosestString = -1;
  lastTuningState = 0;
  musicToolsState = GUITAR_TUNER;

  initMicrophone();
  drawGuitarTuner();
}

void stopGuitarTuner() {
  if (tunerActive) {
    deinitMicrophone();
    tunerActive = false;
  }
}

void drawGuitarTuner() {
  // Draw static UI elements only once
  canvas.fillScreen(TFT_BLACK);

  // Draw status bar first
  drawStatusBar(false);

  // Fill white background AFTER status bar (starts at y=24 to avoid cutting off status bar)
  canvas.fillRect(0, 24, 240, 111, TFT_WHITE);

  // Instructions
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(1);
  canvas.drawString("`] Back", 95, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Update only the dynamic parts of the tuner display
void updateTunerDisplay() {
  // Define update areas - centered above "back" text (which is at y=122)
  const int noteX = 110;
  const int noteY = 65;

  // Determine current tuning state
  int currentTuningState = 0; // 0=in tune, -1=flat, 1=sharp
  if (closestString >= 0) {
    if (centsDiff < -3.0) currentTuningState = -1;
    else if (centsDiff > 3.0) currentTuningState = 1;
  }

  // Only update if something changed (reduce jitter by requiring larger changes)
  static float lastCentsDiff = 0;
  bool needsUpdate = (closestString != lastClosestString) ||
                     (currentTuningState != lastTuningState) ||
                     (abs(centsDiff - lastCentsDiff) > 2.0);  // Only update if cents changed by >2

  if (!needsUpdate) return;

  lastCentsDiff = centsDiff;

  // Clear entire display area (white background, starts after status bar)
  canvas.fillRect(0, 24, 240, 95, TFT_WHITE);

  if (closestString >= 0) {
    // Calculate fill amount based on how far off we are (0-50 cents = 0-100%)
    float flatAmount = 0;
    float sharpAmount = 0;

    if (centsDiff < 0) {
      flatAmount = min(abs(centsDiff) / 50.0, 1.0);  // 50 cents = 100% fill
    } else if (centsDiff > 0) {
      sharpAmount = min(centsDiff / 50.0, 1.0);  // 50 cents = 100% fill
    }

    // Draw FLAT triangle - outline in red, filled based on proximity
    canvas.drawTriangle(70, 65, 50, 80, 70, 95, TFT_RED);
    if (flatAmount > 0) {
      // Fill from bottom up based on how flat we are
      int fillHeight = (int)(30 * flatAmount);  // Triangle height is 30px
      int fillY = 95 - fillHeight;

      // Draw filled portion (simple approximation with rectangles)
      for (int y = fillY; y < 95; y++) {
        int leftX = 50 + (70 - 50) * (y - 65) / 30;
        int rightX = 70;
        if (y >= 65 && y <= 95) {
          if (y < 80) {
            leftX = 50 + (70 - 50) * (80 - y) / 15;
            rightX = 70;
          } else {
            leftX = 50 + (70 - 50) * (y - 80) / 15;
            rightX = 70;
          }
          canvas.drawLine(leftX, y, rightX, y, TFT_RED);
        }
      }
    }

    // Draw SHARP triangle - outline in red, filled based on proximity
    canvas.drawTriangle(170, 65, 190, 80, 170, 95, TFT_RED);
    if (sharpAmount > 0) {
      // Fill from bottom up based on how sharp we are
      int fillHeight = (int)(30 * sharpAmount);  // Triangle height is 30px
      int fillY = 95 - fillHeight;

      // Draw filled portion
      for (int y = fillY; y < 95; y++) {
        int leftX = 170;
        int rightX = 190;
        if (y >= 65 && y <= 95) {
          if (y < 80) {
            rightX = 190 - (190 - 170) * (80 - y) / 15;
          } else {
            rightX = 190 - (190 - 170) * (y - 80) / 15;
          }
          canvas.drawLine(leftX, y, rightX, y, TFT_RED);
        }
      }
    }

    // Draw note/symbol in center (black text on white background)
    canvas.setTextSize(5);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(guitarStrings[closestString].name, noteX, noteY);

    // If in tune, add black outline box
    if (abs(centsDiff) < 3.0) {
      canvas.drawRoundRect(100, 60, 40, 45, 5, TFT_BLACK);
      canvas.drawRoundRect(101, 61, 38, 43, 5, TFT_BLACK);
    }
  } else {
    // No note detected - show "?"
    canvas.setTextSize(5);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("?", 110, noteY);
  }

  // Save current state
  lastClosestString = closestString;
  lastTuningState = currentTuningState;
}

void updateGuitarTuner() {
  if (!tunerActive) return;

  // Sample and detect frequency - fast response
  static unsigned long lastSample = 0;
  if (millis() - lastSample > 30) {  // Sample every 30ms for quick response
    lastSample = millis();
    detectedFrequency = detectFrequency();
    analyzeFrequency(detectedFrequency);

    // Update only changed parts of display
    updateTunerDisplay();
  }
}

// Audio visualizer implementation
void startAudioVisualizer() {
  visualizerActive = true;
  musicToolsState = AUDIO_VISUALIZER;

  // Clear bar heights
  for (int i = 0; i < 20; i++) {
    barHeights[i] = 0;
  }
  barIndex = 0;

  initMicrophone();
  drawAudioVisualizer();
}

void stopAudioVisualizer() {
  if (visualizerActive) {
    deinitMicrophone();
    visualizerActive = false;
  }
}

void drawAudioVisualizer() {
  // Static elements only - draw once
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  // No title - removed

  // Instructions
  canvas.setTextColor(0x780F);  // Deep purple
  canvas.drawString("`] Back", 95, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void updateAudioVisualizer() {
  if (!visualizerActive) return;

  // Equalizer settings
  static const int numBars = 20;
  static const int barWidth = 10;
  static const int barSpacing = 12;
  static const int barMaxHeight = 105;  // Taller since no title
  static const int barsY = 118;
  static const int startX = 5;
  static float smoothBarHeights[20] = {0};  // Use float for smoother interpolation
  static int16_t lastBarHeights[20] = {0};

  // Read audio samples using M5Cardputer's mic - faster updates
  static unsigned long lastSample = 0;
  if (millis() - lastSample > 16) {  // ~60fps updates for smoothness
    lastSample = millis();

    if (M5Cardputer.Mic.isEnabled()) {
      if (M5Cardputer.Mic.record(audioBuffer, BUFFER_SIZE, SAMPLE_RATE)) {
        // Calculate bar heights from audio buffer
        int samplesPerBar = BUFFER_SIZE / numBars;

        for (int i = 0; i < numBars; i++) {
          // Find max amplitude in this frequency band
          int maxAmp = 0;
          int startIdx = i * samplesPerBar;
          int endIdx = (i + 1) * samplesPerBar;

          for (int j = startIdx; j < endIdx && j < BUFFER_SIZE; j++) {
            int amp = abs(audioBuffer[j]);
            if (amp > maxAmp) maxAmp = amp;
          }

          // Convert to bar height
          float targetHeight = (maxAmp * barMaxHeight) / 32768.0;
          if (targetHeight > barMaxHeight) targetHeight = barMaxHeight;

          // Smooth transition with interpolation for buttery smoothness
          if (targetHeight > smoothBarHeights[i]) {
            smoothBarHeights[i] = smoothBarHeights[i] * 0.7 + targetHeight * 0.3;  // Fast rise
          } else {
            smoothBarHeights[i] = smoothBarHeights[i] * 0.85 + targetHeight * 0.15;  // Slower fall
          }
        }

        // Redraw only bars that changed (with anti-flicker)
        for (int i = 0; i < numBars; i++) {
          int x = startX + i * barSpacing;
          int newHeight = (int)smoothBarHeights[i];

          // Only redraw if changed by more than 1px (reduce flicker)
          if (abs(newHeight - lastBarHeights[i]) > 0) {
            // Clear old bar area
            canvas.fillRect(x, barsY - barMaxHeight, barWidth, barMaxHeight, TFT_BLACK);

            // Purple gradient based on height (3 shades)
            uint16_t barColor = 0x780F;  // Deep purple (low)
            if (newHeight > barMaxHeight * 0.4) barColor = 0xA11F;  // Medium purple (mid)
            if (newHeight > barMaxHeight * 0.7) barColor = 0xF81F;  // Bright purple (high)

            // Draw new bar from bottom up
            if (newHeight > 0) {
              canvas.fillRect(x, barsY - newHeight, barWidth, newHeight, barColor);
            }

            lastBarHeights[i] = newHeight;
          }
        }
      }
    }
  }
}

void handleMusicToolsNavigation(char key) {
  if (musicToolsState == TOOLS_MENU) {
    if (key == ';' || key == ',') {
      // Up
      if (musicToolsMenuIndex > 0) {
        musicToolsMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawMusicToolsMenu();
      }
    } else if (key == '.' || key == '/') {
      // Down
      if (musicToolsMenuIndex < 3) {  // Now have 4 items
        musicToolsMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawMusicToolsMenu();
      }
    }
  } else if (musicToolsState == LAB_BEAT_MACHINE) {
    // Pass navigation to LBM
    handleLBMNavigation(key);
  } else if (musicToolsState == TAP_TEMPO) {
    // Pass navigation to Tap Tempo
    handleTapTempoNavigation(key);
  }
}
