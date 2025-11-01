#include "lbm.h"
#include <M5Cardputer.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include "audio_manager.h"

// Embedded 808 samples
#include "samples/sample_808_kick.h"
#include "samples/sample_808_snare.h"
#include "samples/sample_808_hat.h"
#include "samples/sample_808_tom.h"

// Access to screensaver timer (from main.cpp)
extern unsigned long lastActivityTime;

// Track colors from mockups
uint16_t trackColors[LBM_TRACKS] = {
  0xFDA0,  // Track 1: Orange/tan (kick)
  0xF800,  // Track 2: Red (snare)
  0x001F,  // Track 3: Blue (hat)
  0x051F,  // Track 4: Light blue (tom)
  0xFFE0,  // Track 5: Yellow (melody 1)
  0xAFE0,  // Track 6: Yellow-green (melody 2)
  0x07E0,  // Track 7: Green (melody 3)
  0x87F0   // Track 8: Light green (melody 4)
};

// All available drum sounds (can be assigned to any track)
const char* drumSounds[] = {"kick", "snare", "hat", "tom"};
const int numDrumSounds = 4;

// Current sound selection for each track (index into drumSounds array)
// Tracks 0-3 default to their namesake, tracks 4-7 are melody (no sound cycling)
uint8_t trackSound[LBM_TRACKS] = {0, 1, 2, 3, 0, 0, 0, 0};  // kick, snare, hat, tom by default

const char* trackNames[LBM_TRACKS] = {
  "kick", "snare", "hat", "tom",
  "notes", "notes", "notes", "notes"
};

// Helper: Get current sound name for a track
const char* getCurrentSampleName(int track) {
  if (track < 4) {
    // Drum tracks - return selected drum sound
    return drumSounds[trackSound[track]];
  } else {
    // Melody tracks
    return trackNames[track];
  }
}

// Helper: Check if track can cycle sounds
bool canCycleSounds(int track) {
  return track < 4;  // Only first 4 tracks (drums) can cycle
}

// Global state
Pattern currentPattern;
PatternChain patternChain = {{}, 0, 0, false, -1};  // Initialize chain
int currentTrack = 0;
int cursorX = 0;
int cursorY = 0;
bool isPlaying = false;
unsigned long lastStepTime = 0;
int playbackStep = 0;
bool lbmActive = false;

// UI state
enum LBMUIState {
  UI_MAIN,       // Main sequencer view
  UI_SAVE,       // Save dialog
  UI_EXPORT      // Export dialog
};

// Navigation items (list-based, spatial)
enum LBMNavItem {
  NAV_TRACK = 0,   // Top row left
  NAV_SOUND,       // Second row left
  NAV_NUDGE,       // Second row middle
  NAV_SPEED,       // Second row right
  NAV_BPM,         // Top row right (tempo)
  NAV_STEPS,       // Step grid
  NAV_MODE         // POLY/808/USER button
};

LBMUIState uiState = UI_MAIN;
LBMNavItem selectedItem = NAV_SOUND;  // Start on sound
bool editMode = false;  // Editing sound/nudge/speed
bool bpmEditMode = false;  // Editing BPM directly
String bpmInput = "";

// ============================================================================
// INITIALIZATION
// ============================================================================

void enterLBM() {
  lbmActive = true;
  clearPattern();
  currentPattern.bpm = 120;
  currentPattern.volume = 8;  // Default volume (0-10)
  currentPattern.mode = MODE_808;
  currentPattern.resolution = RES_32ND;  // Default to 32nd notes (2x length)
  strcpy(currentPattern.name, "Pattern1");

  // Initialize speeds to 1x
  for (int i = 0; i < LBM_TRACKS; i++) {
    currentPattern.speed[i] = 1.0f;
  }

  selectedItem = NAV_SOUND;  // Start with sound selected
  editMode = false;
  bpmEditMode = false;
  cursorX = 0;
  cursorY = 0;

  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawLBM();
}

void exitLBM() {
  lbmActive = false;
  isPlaying = false;
  M5Cardputer.Display.fillScreen(TFT_WHITE);
}

void clearPattern() {
  for (int t = 0; t < LBM_TRACKS; t++) {
    currentPattern.nudge[t] = 0;
    currentPattern.speed[t] = 1.0f;
    for (int s = 0; s < LBM_STEPS; s++) {
      currentPattern.steps[t][s] = false;
      currentPattern.notes[t][s] = 60; // Middle C
    }
  }
}

// ============================================================================
// DRAWING FUNCTIONS (SELECTIVE UPDATES)
// ============================================================================

void drawTrackHeader() {
  // Clear header area (up to steps)
  M5Cardputer.Display.fillRect(0, 0, 240, 55, TFT_WHITE);

  M5Cardputer.Display.setTextSize(1);
  int startX = 30;  // Left-align with steps

  // ROW 1: Track, BPM, TAP (Y=18) - 2px margin from row 2
  int row1Y = 18;
  int boxY1 = row1Y - 3;

  // Track (with box, enter to engage)
  String trackLabel = "track: " + String(currentTrack + 1);
  int trackWidth = trackLabel.length() * 6 + 8;
  M5Cardputer.Display.fillRoundRect(startX, boxY1, trackWidth, 18, 7, TFT_WHITE);
  M5Cardputer.Display.drawRoundRect(startX, boxY1, trackWidth, 18, 7, TFT_BLACK);
  if (selectedItem == NAV_TRACK) {
    M5Cardputer.Display.drawRoundRect(startX-1, boxY1-1, trackWidth+2, 20, 7, trackColors[currentTrack]);
    M5Cardputer.Display.drawRoundRect(startX-2, boxY1-2, trackWidth+4, 22, 7, trackColors[currentTrack]);
  }
  M5Cardputer.Display.setTextColor(editMode && selectedItem == NAV_TRACK ? trackColors[currentTrack] : TFT_BLACK);
  M5Cardputer.Display.drawString(trackLabel.c_str(), startX+4, row1Y);

  // BPM (show input buffer when editing)
  int bpmX = startX + trackWidth + 8;
  String bpmLabel;
  if (bpmEditMode) {
    bpmLabel = bpmInput + "_";  // Show typing cursor
    M5Cardputer.Display.setTextColor(TFT_RED);  // Red when editing
  } else {
    bpmLabel = String(currentPattern.bpm) + " BPM";
    M5Cardputer.Display.setTextColor(selectedItem == NAV_BPM ? trackColors[currentTrack] : TFT_BLACK);
  }
  M5Cardputer.Display.drawString(bpmLabel.c_str(), bpmX, row1Y);

  // ROW 2: Sound, Nudge, Speed (Y=38) - 2px margin to steps
  int row2Y = 38;
  int boxY = row2Y - 3;

  // Sound pill - show current sample variant
  String sampleLabel = "< " + String(getCurrentSampleName(currentTrack)) + " >";
  int labelWidth = sampleLabel.length() * 6 + 10;
  M5Cardputer.Display.fillRoundRect(startX, boxY, labelWidth, 18, 9, trackColors[currentTrack]);
  if (selectedItem == NAV_SOUND) {
    M5Cardputer.Display.drawRoundRect(startX-1, boxY-1, labelWidth+2, 20, 9, TFT_BLACK);
    M5Cardputer.Display.drawRoundRect(startX-2, boxY-2, labelWidth+4, 22, 9, TFT_BLACK);
  }
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString(sampleLabel.c_str(), startX+5, row2Y);

  // Nudge
  int nudgeX = startX + labelWidth + 8;
  String nudgeLabel = "nudge:" + String(currentPattern.nudge[currentTrack]);
  int nudgeWidth = nudgeLabel.length() * 6 + 8;
  M5Cardputer.Display.fillRoundRect(nudgeX, boxY, nudgeWidth, 18, 7, TFT_WHITE);
  M5Cardputer.Display.drawRoundRect(nudgeX, boxY, nudgeWidth, 18, 7, TFT_BLACK);
  if (selectedItem == NAV_NUDGE) {
    M5Cardputer.Display.drawRoundRect(nudgeX-1, boxY-1, nudgeWidth+2, 20, 7, trackColors[currentTrack]);
    M5Cardputer.Display.drawRoundRect(nudgeX-2, boxY-2, nudgeWidth+4, 22, 7, trackColors[currentTrack]);
  }
  M5Cardputer.Display.setTextColor(editMode && selectedItem == NAV_NUDGE ? trackColors[currentTrack] : TFT_BLACK);
  M5Cardputer.Display.drawString(nudgeLabel.c_str(), nudgeX+4, row2Y);

  // Speed
  int speedX = nudgeX + nudgeWidth + 8;
  String speedLabel = String(currentPattern.speed[currentTrack], 1) + "x";
  int speedWidth = speedLabel.length() * 6 + 8;
  M5Cardputer.Display.fillRoundRect(speedX, boxY, speedWidth, 18, 7, TFT_WHITE);
  M5Cardputer.Display.drawRoundRect(speedX, boxY, speedWidth, 18, 7, TFT_BLACK);
  if (selectedItem == NAV_SPEED) {
    M5Cardputer.Display.drawRoundRect(speedX-1, boxY-1, speedWidth+2, 20, 7, trackColors[currentTrack]);
    M5Cardputer.Display.drawRoundRect(speedX-2, boxY-2, speedWidth+4, 22, 7, trackColors[currentTrack]);
  }
  M5Cardputer.Display.setTextColor(editMode && selectedItem == NAV_SPEED ? trackColors[currentTrack] : TFT_BLACK);
  M5Cardputer.Display.drawString(speedLabel.c_str(), speedX+4, row2Y);
}

void drawStepGrid() {
  // Clear step area (start after sound row boxes, end before MODE button)
  M5Cardputer.Display.fillRect(0, 54, 240, 33, TFT_WHITE);

  // Draw 8x2 grid of steps (16 total)
  int startX = 30;  // Left-aligned with sound
  int startY = 56;  // Below controls with margin (1px lower)
  int stepWidth = 20;
  int stepHeight = 14;
  int spacing = 2;

  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      int stepIndex = row * 8 + col;
      int x = startX + col * (stepWidth + spacing);
      int y = startY + row * (stepHeight + spacing);

      // Fill only when step is toggled ON (active in pattern)
      uint16_t fillColor = TFT_WHITE;  // Default: white background
      bool isSelected = (selectedItem == NAV_STEPS && col == cursorX && row == cursorY);

      if (currentPattern.steps[currentTrack][stepIndex]) {
        fillColor = trackColors[currentTrack];  // Filled = toggled ON
      }

      // Draw step (rounder corners)
      M5Cardputer.Display.fillRoundRect(x, y, stepWidth, stepHeight, 7, fillColor);

      // Draw outline (black)
      M5Cardputer.Display.drawRoundRect(x, y, stepWidth, stepHeight, 7, TFT_BLACK);

      // Draw selection indicator with track color outline
      if (isSelected) {
        M5Cardputer.Display.drawRoundRect(x-1, y-1, stepWidth+2, stepHeight+2, 7, trackColors[currentTrack]);
        M5Cardputer.Display.drawRoundRect(x-2, y-2, stepWidth+4, stepHeight+4, 7, trackColors[currentTrack]);
      }

      // Draw step number (1-16) in white text
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      String stepNum = String(stepIndex + 1);
      int textX = x + (stepWidth / 2) - (stepNum.length() * 3);
      int textY = y + (stepHeight / 2) - 4;
      M5Cardputer.Display.drawString(stepNum.c_str(), textX, textY);

      // Draw playback indicator if playing
      if (isPlaying && stepIndex == playbackStep) {
        M5Cardputer.Display.drawRoundRect(x+1, y+1, stepWidth-2, stepHeight-2, 5, TFT_RED);
      }
    }
  }
}

void drawSoundBanks() {
  // Mode button left-aligned with steps (Y=88) - 2px margin from steps
  M5Cardputer.Display.fillRect(0, 88, 240, 22, TFT_WHITE);

  int btnY = 88;
  int btnW = 45;
  int btnH = 18;
  int startX = 30;  // Left-align with steps

  // Determine mode label
  const char* modeLabel = (currentPattern.mode == MODE_808) ? "808" : "USER";

  M5Cardputer.Display.fillRoundRect(startX, btnY, btnW, btnH, 8, TFT_WHITE);
  M5Cardputer.Display.drawRoundRect(startX, btnY, btnW, btnH, 8, TFT_BLACK);

  // Highlight if selected
  if (selectedItem == NAV_MODE) {
    M5Cardputer.Display.drawRoundRect(startX-1, btnY-1, btnW+2, btnH+2, 8, trackColors[currentTrack]);
    M5Cardputer.Display.drawRoundRect(startX-2, btnY-2, btnW+4, btnH+4, 8, trackColors[currentTrack]);
  }

  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString(modeLabel, startX+10, btnY + 5);
}

void drawControls() {
  // Help text at bottom, centered (Y=115)
  M5Cardputer.Display.fillRect(0, 115, 240, 15, TFT_WHITE);
  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.setTextSize(1);
  String helpText = "SPACE=play s=save l=load c=chain";
  int textWidth = helpText.length() * 6;
  int centerX = (240 - textWidth) / 2;
  M5Cardputer.Display.drawString(helpText.c_str(), centerX, 117);
}


void drawLBM() {
  if (uiState == UI_MAIN) {
    drawTrackHeader();
    drawStepGrid();
    drawSoundBanks();
    drawControls();
  }
}

// ============================================================================
// AUDIO SYNTHESIS (POLY MODE)
// ============================================================================

void playPOLYSound(TrackType track, uint8_t note) {
  switch(track) {
    case TRACK_KICK:
      // Kick: 100Hz → 40Hz sweep
      for (int i = 0; i < 8; i++) {
        int freq = 100 - (i * 8);
        M5Cardputer.Speaker.tone(freq, 10);
        delay(10);
      }
      M5Cardputer.Speaker.end();
      break;

    case TRACK_SNARE:
      // Snare: Rapid noise simulation
      for (int i = 0; i < 6; i++) {
        M5Cardputer.Speaker.tone(random(8000, 12000), 8);
        delay(8);
      }
      M5Cardputer.Speaker.end();
      break;

    case TRACK_HAT:
      // Hi-hat: Short high burst
      M5Cardputer.Speaker.tone(10000, 30);
      M5Cardputer.Speaker.end();
      break;

    case TRACK_TOM:
      // Tom: 200Hz decay
      for (int i = 0; i < 10; i++) {
        M5Cardputer.Speaker.tone(200 - (i * 5), 10);
        delay(10);
      }
      M5Cardputer.Speaker.end();
      break;

    default:
      // Melody tracks: MIDI note to frequency
      float freq = 440.0 * pow(2.0, (note - 69) / 12.0);
      M5Cardputer.Speaker.tone((int)freq, 100);
      delay(100);
      M5Cardputer.Speaker.end();
      break;
  }
}

// Static buffer for WAV playback (reused across samples)
static int16_t* sampleBuffer = nullptr;
static size_t sampleBufferSize = 0;

void playMP3Sample(const char* basePath) {
  // Play short audio sample (WAV or MP3) for 808/USER modes
  // Try WAV first, then MP3
  String path = String(basePath);

  Serial.printf("Trying to play: %s\n", path.c_str());

  // Check if path already has extension
  if (!path.endsWith(".wav") && !path.endsWith(".mp3")) {
    Serial.println("No valid extension!");
    return;
  }

  // Try the path as-is first
  if (!SD.exists(path.c_str())) {
    Serial.printf("File not found: %s, trying alternate extension...\n", path.c_str());

    // If WAV doesn't exist, try MP3
    if (path.endsWith(".wav")) {
      path.replace(".wav", ".mp3");
    }
    // If MP3 doesn't exist, try WAV
    else if (path.endsWith(".mp3")) {
      path.replace(".mp3", ".wav");
    }

    Serial.printf("Now trying: %s\n", path.c_str());
  }

  // Play if file exists
  if (SD.exists(path.c_str())) {
    Serial.printf("Found file: %s\n", path.c_str());

    // For WAV files, load and play entire sample
    if (path.endsWith(".wav")) {
      File audioFile = SD.open(path.c_str());
      if (audioFile) {
        size_t fileSize = audioFile.size();
        Serial.printf("File size: %d bytes\n", fileSize);

        // Read WAV header
        audioFile.seek(22);
        uint16_t numChannels = 0;
        audioFile.read((uint8_t*)&numChannels, 2);
        Serial.printf("Channels: %d\n", numChannels);

        audioFile.seek(24);
        uint32_t sampleRate = 0;
        audioFile.read((uint8_t*)&sampleRate, 4);
        Serial.printf("Sample rate: %d Hz\n", sampleRate);

        audioFile.seek(34);
        uint16_t bitsPerSample = 0;
        audioFile.read((uint8_t*)&bitsPerSample, 2);
        Serial.printf("Bits per sample: %d\n", bitsPerSample);

        // Skip to audio data (skip 44-byte header)
        audioFile.seek(44);

        // Calculate audio data size (limit to 64KB)
        size_t dataSize = fileSize - 44;
        if (dataSize > 65536) dataSize = 65536;

        // Allocate or reuse static buffer
        if (sampleBuffer == nullptr || sampleBufferSize < dataSize) {
          if (sampleBuffer != nullptr) {
            free(sampleBuffer);
          }
          sampleBuffer = (int16_t*)malloc(dataSize);
          sampleBufferSize = dataSize;
          Serial.printf("Allocated buffer: %d bytes\n", dataSize);
        }

        if (sampleBuffer) {
          size_t bytesRead = audioFile.read((uint8_t*)sampleBuffer, dataSize);
          Serial.printf("Read %d bytes\n", bytesRead);

          if (bytesRead > 0) {
            // Convert 24-bit to 16-bit if needed
            size_t numSamples;
            if (bitsPerSample == 24) {
              // Convert 24-bit to 16-bit (take upper 16 bits of each 24-bit sample)
              uint8_t* rawData = (uint8_t*)sampleBuffer;
              numSamples = bytesRead / 3;  // 3 bytes per 24-bit sample

              for (size_t i = 0; i < numSamples; i++) {
                // Read 3 bytes (24-bit sample, little-endian)
                int32_t sample24 = (rawData[i*3] << 8) | (rawData[i*3+1] << 16) | (rawData[i*3+2] << 24);
                // Convert to 16-bit by taking upper 16 bits
                sampleBuffer[i] = (int16_t)(sample24 >> 16);
              }
              Serial.printf("Converted %d samples from 24-bit to 16-bit\n", numSamples);
            } else {
              // Already 16-bit
              numSamples = bytesRead / 2;
            }

            // Play using M5Unified speaker (blocking - wait for current sample to finish)
            while (M5Cardputer.Speaker.isPlaying()) {
              delay(1);
            }

            Serial.println("Playing...");
            M5Cardputer.Speaker.playRaw(sampleBuffer, numSamples, sampleRate, false, 1, 0);
          }
        } else {
          Serial.println("Failed to allocate buffer!");
        }

        audioFile.close();
      } else {
        Serial.println("Failed to open file!");
      }
    } else {
      // MP3 not supported for samples - fallback to POLY
      // (MP3 playback is too slow for real-time triggering)
    }
  } else {
    Serial.printf("File not found: %s\n", path.c_str());
  }
}

void play808Sound(TrackType track, uint8_t note) {
  // Play embedded 808 samples from PROGMEM
  const int16_t* sampleData = nullptr;
  uint32_t sampleLength = 0;
  uint32_t sampleRate = 0;

  // Use trackSound[] to determine which drum sound to play
  uint8_t soundIndex = (track < 4) ? trackSound[track] : track;

  switch(soundIndex) {
    case 0:  // Kick
      sampleData = sample_808_kick_data;
      sampleLength = sample_808_kick_length;
      sampleRate = sample_808_kick_rate;
      break;
    case 1:  // Snare
      sampleData = sample_808_snare_data;
      sampleLength = sample_808_snare_length;
      sampleRate = sample_808_snare_rate;
      break;
    case 2:  // Hat
      sampleData = sample_808_hat_data;
      sampleLength = sample_808_hat_length;
      sampleRate = sample_808_hat_rate;
      break;
    case 3:  // Tom
      sampleData = sample_808_tom_data;
      sampleLength = sample_808_tom_length;
      sampleRate = sample_808_tom_rate;
      break;
    default:
      // Melody tracks use POLY mode for now
      playPOLYSound(track, note);
      return;
  }

  if (sampleData) {
    // Stop any current playback and start immediately (no latency)
    M5Cardputer.Speaker.stop();

    // Play directly from PROGMEM - M5Unified supports this
    // Volume range: 0-10 from pattern, convert to 0-255 for speaker
    // Kick gets extra 1.6x boost (384/10 = 38.4 per level)
    uint8_t speakerVol;
    if (track == TRACK_KICK) {
      speakerVol = min(255, (currentPattern.volume * 384) / 10);  // Kick: 1.6x boost
    } else {
      speakerVol = min(255, (currentPattern.volume * 320) / 10);  // Others: 1.25x boost
    }
    M5Cardputer.Speaker.playRaw(sampleData, sampleLength, sampleRate, false, 1, speakerVol);
  }
}

void playUSERSound(TrackType track, uint8_t note) {
  const char* samplePath = nullptr;

  // Use trackSound[] to determine which drum sound to play
  if (track < 4) {
    uint8_t soundIndex = trackSound[track];
    switch(soundIndex) {
      case 0: samplePath = "/mp3s/lbm/user/kick.wav";  break;
      case 1: samplePath = "/mp3s/lbm/user/snare.wav"; break;
      case 2: samplePath = "/mp3s/lbm/user/hat.wav";   break;
      case 3: samplePath = "/mp3s/lbm/user/tom.wav";   break;
    }
  } else {
    // Melody tracks use POLY mode for now
    playPOLYSound(track, note);
    return;
  }

  if (samplePath) {
    playMP3Sample(samplePath);
  }
}

void triggerTrack(int track) {
  if (currentPattern.mode == MODE_808) {
    play808Sound((TrackType)track, currentPattern.notes[track][0]);
  } else {
    playUSERSound((TrackType)track, currentPattern.notes[track][0]);
  }
}

void playStep(int track, int step) {
  if (currentPattern.steps[track][step]) {
    if (currentPattern.mode == MODE_808) {
      play808Sound((TrackType)track, currentPattern.notes[track][step]);
    } else {
      playUSERSound((TrackType)track, currentPattern.notes[track][step]);
    }
  }
}

// ============================================================================
// PLAYBACK ENGINE
// ============================================================================

void updateLBM() {
  if (!lbmActive) return;

  // Handle playback timing
  if (isPlaying) {
    // Prevent screensaver during playback
    lastActivityTime = millis();

    // Calculate step duration based on resolution
    // 16th notes: 4 steps per quarter note
    // 32nd notes: 8 steps per quarter note (half the duration)
    int stepsPerQuarterNote = (currentPattern.resolution == RES_32ND) ? 8 : 4;
    unsigned long stepDuration = (60000 / currentPattern.bpm) / stepsPerQuarterNote;

    if (millis() - lastStepTime >= stepDuration) {
      lastStepTime = millis();

      // Play all active tracks at this step
      for (int t = 0; t < LBM_TRACKS; t++) {
        playStep(t, playbackStep);
      }

      // Advance playback
      playbackStep = (playbackStep + 1) % LBM_STEPS;

      // Check if pattern completed (looped back to start)
      if (playbackStep == 0 && patternChain.enabled && patternChain.length > 1) {
        // Pattern chaining logic
        patternChain.currentIndex = (patternChain.currentIndex + 1) % patternChain.length;

        // Load next pattern in chain
        String filename = String(patternChain.patterns[patternChain.currentIndex]) + ".lbm";
        loadPattern(filename.c_str());

        Serial.printf("Chain: Loaded pattern %d/%d: %s\n",
                      patternChain.currentIndex + 1,
                      patternChain.length,
                      filename.c_str());
      }

      // Redraw only the step grid (selective update)
      drawStepGrid();
    }
  }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void handleLBMNavigation(char key) {
  // BPM edit mode - handle typing
  if (bpmEditMode) {
    if (key >= '0' && key <= '9') {
      // Limit to 3 digits max
      if (bpmInput.length() < 3) {
        bpmInput += key;
        drawTrackHeader();
      }
    } else if (key == '\b') {
      // Backspace - delete last character
      if (bpmInput.length() > 0) {
        bpmInput.remove(bpmInput.length() - 1);
        drawTrackHeader();
      }
    } else if (key == '\n') {
      // Apply BPM (only if input is valid)
      if (bpmInput.length() > 0) {
        int newBPM = bpmInput.toInt();
        if (newBPM >= LBM_MIN_BPM && newBPM <= LBM_MAX_BPM) {
          currentPattern.bpm = newBPM;
        }
      }
      bpmInput = "";
      bpmEditMode = false;
      drawTrackHeader();
    } else if (key == 27) { // ESC - cancel without changing
      bpmInput = "";
      bpmEditMode = false;
      drawTrackHeader();
    }
    return;
  }

  // Main sequencer controls
  if (key == 27 || key == '`') { // ESC or backtick
    isPlaying = false;
    editMode = false;  // Cancel any edit mode
  }
  else if (key == ' ') {
    // Play/pause
    isPlaying = !isPlaying;
    if (isPlaying) {
      playbackStep = 0;
      lastStepTime = millis();
    }
    drawStepGrid();
  }
  // UP ARROW - Spatial navigation
  else if (key == ';') {
    if (editMode) return;  // No navigation in edit mode

    if (selectedItem == NAV_STEPS) {
      // From steps, go up to sound/nudge/speed depending on X position
      if (cursorY > 0) {
        cursorY--;  // Move between step rows first
        drawStepGrid();
      } else {
        // At top row of steps - go to controls above
        if (cursorX <= 2) selectedItem = NAV_SOUND;
        else if (cursorX <= 5) selectedItem = NAV_NUDGE;
        else selectedItem = NAV_SPEED;
        drawLBM();
      }
    } else if (selectedItem == NAV_SOUND || selectedItem == NAV_NUDGE || selectedItem == NAV_SPEED) {
      selectedItem = NAV_TRACK;
      drawLBM();
    } else if (selectedItem == NAV_BPM) {
      selectedItem = NAV_TRACK;
      drawLBM();
    } else if (selectedItem == NAV_MODE) {
      selectedItem = NAV_STEPS;
      cursorY = 1;  // Go to bottom row
      drawLBM();
    }
  }
  // DOWN ARROW - Spatial navigation
  else if (key == '.') {
    if (editMode) return;  // No navigation in edit mode

    if (selectedItem == NAV_TRACK) {
      selectedItem = NAV_SOUND;
      drawLBM();
    } else if (selectedItem == NAV_SOUND || selectedItem == NAV_NUDGE || selectedItem == NAV_SPEED || selectedItem == NAV_BPM) {
      selectedItem = NAV_STEPS;
      cursorY = 0;  // Start at top row
      drawLBM();
    } else if (selectedItem == NAV_STEPS) {
      // Move down between step rows
      if (cursorY < 1) {
        cursorY++;
        drawStepGrid();
      } else {
        // At bottom row - go to mode button
        selectedItem = NAV_MODE;
        drawLBM();
      }
    }
  }
  // LEFT ARROW - Spatial navigation / Edit
  else if (key == ',') {
    if (editMode) {
      // Edit mode - adjust values
      if (selectedItem == NAV_TRACK) {
        currentTrack = (currentTrack - 1 + LBM_TRACKS) % LBM_TRACKS;
        drawLBM();
      } else if (selectedItem == NAV_SOUND) {
        // Cycle to previous drum sound (kick → tom → hat → snare → kick)
        if (canCycleSounds(currentTrack)) {
          trackSound[currentTrack] = (trackSound[currentTrack] - 1 + numDrumSounds) % numDrumSounds;
          drawTrackHeader();
        }
      } else if (selectedItem == NAV_NUDGE) {
        if (currentPattern.nudge[currentTrack] > 0) {
          currentPattern.nudge[currentTrack]--;
          drawTrackHeader();
        }
      } else if (selectedItem == NAV_SPEED) {
        if (currentPattern.speed[currentTrack] > 0.5f) {
          currentPattern.speed[currentTrack] -= 0.5f;
          drawTrackHeader();
        }
      }
    } else {
      // Navigation mode
      if (selectedItem == NAV_NUDGE) {
        selectedItem = NAV_SOUND;
        drawLBM();
      } else if (selectedItem == NAV_SPEED) {
        selectedItem = NAV_NUDGE;
        drawLBM();
      } else if (selectedItem == NAV_STEPS) {
        if (cursorX > 0) {
          cursorX--;
          drawStepGrid();
        }
      }
    }
  }
  // RIGHT ARROW - Spatial navigation / Edit
  else if (key == '/') {
    if (editMode) {
      // Edit mode - adjust values
      if (selectedItem == NAV_TRACK) {
        currentTrack = (currentTrack + 1) % LBM_TRACKS;
        drawLBM();
      } else if (selectedItem == NAV_SOUND) {
        // Cycle to next drum sound (kick → snare → hat → tom → kick)
        if (canCycleSounds(currentTrack)) {
          trackSound[currentTrack] = (trackSound[currentTrack] + 1) % numDrumSounds;
          drawTrackHeader();
        }
      } else if (selectedItem == NAV_NUDGE) {
        if (currentPattern.nudge[currentTrack] < 4) {
          currentPattern.nudge[currentTrack]++;
          drawTrackHeader();
        }
      } else if (selectedItem == NAV_SPEED) {
        if (currentPattern.speed[currentTrack] < 2.0f) {
          currentPattern.speed[currentTrack] += 0.5f;
          drawTrackHeader();
        }
      }
    } else {
      // Navigation mode
      if (selectedItem == NAV_TRACK) {
        selectedItem = NAV_BPM;
        drawLBM();
      } else if (selectedItem == NAV_SOUND) {
        selectedItem = NAV_NUDGE;
        drawLBM();
      } else if (selectedItem == NAV_NUDGE) {
        selectedItem = NAV_SPEED;
        drawLBM();
      } else if (selectedItem == NAV_STEPS) {
        if (cursorX < 7) {
          cursorX++;
          drawStepGrid();
        }
      }
    }
  }
  // ENTER - Toggle/Engage/Confirm
  else if (key == '\n') {
    if (selectedItem == NAV_TRACK || selectedItem == NAV_SOUND || selectedItem == NAV_NUDGE || selectedItem == NAV_SPEED) {
      // Toggle edit mode
      editMode = !editMode;
      drawTrackHeader();
    } else if (selectedItem == NAV_STEPS) {
      // Toggle step
      int stepIndex = cursorY * 8 + cursorX;
      currentPattern.steps[currentTrack][stepIndex] = !currentPattern.steps[currentTrack][stepIndex];

      // Auto-advance cursor
      if (cursorX < 7) {
        cursorX++;
      } else if (cursorY == 0) {
        cursorX = 0;
        cursorY = 1;
      } else {
        cursorX = 0;
        cursorY = 0;
      }
      drawStepGrid();
    } else if (selectedItem == NAV_BPM) {
      // Enter BPM edit mode (inline) - start with empty input
      bpmEditMode = true;
      bpmInput = "";
      drawTrackHeader();
    } else if (selectedItem == NAV_MODE) {
      // Toggle between 808 and USER
      if (currentPattern.mode == MODE_808) {
        currentPattern.mode = MODE_USER;
      } else {
        currentPattern.mode = MODE_808;
      }
      drawSoundBanks();
    }
  }
  else if (key == 'b') {
    // Quick BPM edit shortcut - start with empty input
    selectedItem = NAV_BPM;
    bpmEditMode = true;
    bpmInput = "";
    drawLBM();
  }
  else if (key == 's' && !bpmEditMode && !editMode) {
    // Save pattern (quick save to slot 1)
    String filename = String(currentPattern.name) + ".lbm";
    savePattern(filename.c_str());
  }
  else if (key == 'l' && !bpmEditMode && !editMode) {
    // Load pattern (load from slot 1)
    String filename = String(currentPattern.name) + ".lbm";
    loadPattern(filename.c_str());
  }
  else if (key == 'c' && !bpmEditMode && !editMode) {
    // Toggle chain mode (simple 2-pattern chain for now)
    patternChain.enabled = !patternChain.enabled;

    if (patternChain.enabled) {
      // Setup simple chain: Pattern1 -> Pattern2
      strcpy(patternChain.patterns[0], "Pattern1");
      strcpy(patternChain.patterns[1], "Pattern2");
      patternChain.length = 2;
      patternChain.currentIndex = 0;

      // Visual feedback
      M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_MAGENTA);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.drawString("CHAIN ON: Pattern1->Pattern2", 10, 5);
    } else {
      // Visual feedback
      M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_DARKGREY);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.drawString("CHAIN OFF", 10, 5);
    }

    delay(1000);
    drawLBM();
  }
  else if (key == '+' || key == '=') {
    // Increase volume
    if (currentPattern.volume < 10) {
      currentPattern.volume++;
      // Show volume indicator
      M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_BLACK);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.drawString("VOL: " + String(currentPattern.volume), 10, 5);
    }
  }
  else if (key == '-' || key == '_') {
    // Decrease volume
    if (currentPattern.volume > 0) {
      currentPattern.volume--;
      // Show volume indicator
      M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_BLACK);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.drawString("VOL: " + String(currentPattern.volume), 10, 5);
    }
  }
  else if (key >= '1' && key <= '8') {
    // Live trigger tracks
    int track = key - '1';
    triggerTrack(track);
  }
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void savePattern(const char* filename) {
  // Save pattern to SD card in binary format
  String fullPath = "/lbm_patterns/" + String(filename);

  // Create directory if it doesn't exist
  if (!SD.exists("/lbm_patterns")) {
    SD.mkdir("/lbm_patterns");
  }

  File file = SD.open(fullPath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to save pattern: " + fullPath);
    return;
  }

  // Write pattern struct directly (binary format for speed/size)
  file.write((uint8_t*)&currentPattern, sizeof(Pattern));
  file.close();

  Serial.println("Pattern saved: " + fullPath);

  // Show confirmation on screen
  M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_GREEN);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString("SAVED: " + String(filename), 10, 5);
  delay(1000);
  drawLBM();
}

void loadPattern(const char* filename) {
  // Load pattern from SD card
  String fullPath = "/lbm_patterns/" + String(filename);

  if (!SD.exists(fullPath)) {
    Serial.println("Pattern not found: " + fullPath);

    // Show error
    M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_RED);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("NOT FOUND: " + String(filename), 10, 5);
    delay(1000);
    drawLBM();
    return;
  }

  File file = SD.open(fullPath, FILE_READ);
  if (!file) {
    Serial.println("Failed to load pattern: " + fullPath);
    return;
  }

  // Read pattern struct directly
  file.read((uint8_t*)&currentPattern, sizeof(Pattern));
  file.close();

  Serial.println("Pattern loaded: " + fullPath);

  // Show confirmation
  M5Cardputer.Display.fillRect(0, 0, 240, 20, TFT_CYAN);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString("LOADED: " + String(filename), 10, 5);
  delay(1000);
  drawLBM();
}
