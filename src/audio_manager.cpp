#include "audio_manager.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>

// Shared audio hardware (ONE instance for entire system)
static AudioOutputI2S *sharedAudioOutput = nullptr;
static int masterVolume = 50; // 0-100 (default 50%)
static int savedVolumeBeforeMute = -1; // -1 means not muted

// Current audio source
static AudioSource currentSource = AUDIO_SOURCE_NONE;

// Music player state
static AudioGeneratorMP3 *musicMP3 = nullptr;
static AudioFileSourceSD *musicFile = nullptr;
static String currentMusicPath = "";

// Radio player state
static AudioGeneratorMP3 *radioMP3 = nullptr;
static AudioFileSourceHTTPStream *radioStream = nullptr;

void initAudioManager() {
  // Only initialize once - don't delete/recreate (causes I2S issues)
  if (sharedAudioOutput != nullptr) {
    Serial.println("Audio Manager: Already initialized, skipping");
    return;
  }

  // CRITICAL: Stop M5Speaker to release I2S port 0
  M5Cardputer.Speaker.end();
  Serial.println("Audio Manager: Stopped M5Speaker to release I2S port 0");

  sharedAudioOutput = new AudioOutputI2S(0, 0);  // I2S port 0, internal DAC mode
  sharedAudioOutput->SetPinout(41, 43, 42); // M5Cardputer I2S pins (BCLK, LRC, DOUT)
  sharedAudioOutput->SetOutputModeMono(true);
  sharedAudioOutput->SetGain(masterVolume / 25.0);
  Serial.println("Audio Manager: Initialized AudioOutputI2S on port 0 (internal DAC)");
}

AudioOutputI2S* getSharedAudioOutput() {
  if (sharedAudioOutput == nullptr) {
    initAudioManager();
  }
  return sharedAudioOutput;
}

AudioSource getCurrentAudioSource() {
  return currentSource;
}

// Volume control
void setMasterVolume(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  masterVolume = vol;

  if (sharedAudioOutput) {
    sharedAudioOutput->SetGain(masterVolume / 25.0);
  }
}

int getMasterVolume() {
  return masterVolume;
}

void masterVolumeUp() {
  setMasterVolume(masterVolume + 5);
}

void masterVolumeDown() {
  setMasterVolume(masterVolume - 5);
}

// Temporary mute for navigation (avoids hearing skips during button presses)
void temporarilyMuteAudio() {
  if (savedVolumeBeforeMute == -1) {  // Only save if not already muted
    savedVolumeBeforeMute = masterVolume;
    if (sharedAudioOutput) {
      sharedAudioOutput->SetGain(0.0);
    }
  }
}

void restoreAudioVolume() {
  if (savedVolumeBeforeMute != -1) {  // Only restore if we have a saved volume
    if (sharedAudioOutput) {
      sharedAudioOutput->SetGain(savedVolumeBeforeMute / 25.0);
    }
    savedVolumeBeforeMute = -1;  // Clear saved volume
  }
}

// Music player implementation
bool startMusicPlayback(const String& path) {
  Serial.println("========================================");
  Serial.printf("startMusicPlayback() called with: %s\n", path.c_str());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.flush();

  // Stop any existing audio first
  Serial.println("Step 1: Stopping existing audio...");
  Serial.flush();
  stopMusicPlayback();

  Serial.println("Step 2: Stopping radio stream...");
  Serial.flush();
  stopRadioStream();

  // CRITICAL: Allow time for resources to fully release
  delay(150);
  Serial.println("Step 3: Waited for cleanup");
  Serial.flush();

  // CRITICAL: M5Cardputer.update() may have re-enabled Speaker - force it off again
  M5Cardputer.Speaker.end();
  Serial.println("Step 4: Force-stopped M5Speaker before playback");
  Serial.flush();

  // Initialize audio manager if needed
  initAudioManager();
  Serial.println("Step 5: Audio manager initialized");
  Serial.flush();

  // Ensure we have the audio output though
  if (sharedAudioOutput == nullptr) {
    Serial.println("CRITICAL: sharedAudioOutput is NULL! Audio was never initialized!");
    return false;
  }

  // DON'T reinitialize SD card here - it's already initialized in setup()
  // Check if file exists (SD card is already mounted, so this won't re-init SPI)
  if (!SD.exists(path.c_str())) {
    Serial.printf("ERROR: Audio file not found: %s\n", path.c_str());
    Serial.flush();
    return false;
  }
  Serial.println("Step 4: File exists!");
  Serial.flush();

  Serial.println("Step 5: Creating AudioFileSourceSD...");
  Serial.printf("  Free heap before: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.flush();

  musicFile = new AudioFileSourceSD(path.c_str());

  Serial.println("Step 5: AudioFileSourceSD created (checking validity...)");
  Serial.flush();

  if (!musicFile) {
    Serial.println("ERROR: Failed to create AudioFileSourceSD (null pointer)!");
    Serial.flush();
    return false;
  }

  // Check if file was actually opened
  Serial.println("Step 6: Checking if file opened...");
  Serial.flush();
  if (!musicFile->isOpen()) {
    Serial.println("ERROR: AudioFileSourceSD failed to open file!");
    Serial.flush();
    delete musicFile;
    musicFile = nullptr;
    return false;
  }

  uint32_t fileSize = musicFile->getSize();
  Serial.printf("Step 6: File opened! Size: %d bytes\n", fileSize);

  // CRITICAL: Validate MP3 header to prevent freeze on corrupt files
  if (fileSize < 128) {
    Serial.println("ERROR: File too small to be valid MP3!");
    delete musicFile;
    musicFile = nullptr;
    return false;
  }

  // Read first 4 bytes to check for MP3 sync word (0xFF 0xFB or 0xFF 0xFA)
  uint8_t header[4];
  if (musicFile->read(header, 4) != 4) {
    Serial.println("ERROR: Could not read MP3 header!");
    delete musicFile;
    musicFile = nullptr;
    return false;
  }

  // Check for valid MP3 frame sync (first 11 bits should be all 1s)
  if (header[0] != 0xFF || (header[1] & 0xE0) != 0xE0) {
    Serial.printf("ERROR: Invalid MP3 header: %02X %02X %02X %02X\n",
                  header[0], header[1], header[2], header[3]);
    Serial.println("  This file may be corrupted or not a valid MP3!");
    delete musicFile;
    musicFile = nullptr;
    return false;
  }

  // Rewind to start after header check
  musicFile->seek(0, SEEK_SET);
  Serial.println("Step 7: MP3 header validated - file is likely playable");
  Serial.flush();

  // Create MP3 decoder fresh every time (prevents stale state issues)
  Serial.println("Step 8: Creating AudioGeneratorMP3...");
  Serial.printf("  Free heap before: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.flush();

  musicMP3 = new AudioGeneratorMP3();

  Serial.println("Step 8: AudioGeneratorMP3 created (checking validity...)");
  Serial.flush();

  if (!musicMP3) {
    Serial.println("ERROR: Failed to create AudioGeneratorMP3 (null pointer)!");
    Serial.flush();
    delete musicFile;
    musicFile = nullptr;
    return false;
  }
  Serial.println("Step 7: MP3 decoder created successfully!");
  Serial.flush();

  Serial.println("Calling musicMP3->begin()...");
  Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("  musicFile = %p (open=%s, size=%d)\n", musicFile,
                musicFile->isOpen() ? "YES" : "NO", musicFile->getSize());
  Serial.printf("  sharedAudioOutput = %p\n", sharedAudioOutput);

  // CRITICAL: Feed watchdog before potentially blocking begin() call
  Serial.println("  Feeding watchdog before begin()...");
  yield();
  delay(1);  // Allow other tasks to run

  Serial.println("  Calling begin() NOW...");
  Serial.flush();  // Ensure all serial output is sent before potentially freezing

  bool beginResult = musicMP3->begin(musicFile, sharedAudioOutput);

  Serial.println("  begin() RETURNED!");  // If you see this, begin() didn't freeze
  Serial.flush();
  Serial.printf("  Free heap after begin(): %d bytes\n", ESP.getFreeHeap());
  Serial.printf("  musicMP3->begin() returned: %s\n", beginResult ? "true" : "false");

  if (beginResult) {
    currentMusicPath = path;
    currentSource = AUDIO_SOURCE_MUSIC;
    Serial.println("SUCCESS: MP3 playback started!");
    Serial.println("Calling musicMP3->isRunning()...");
    bool running = musicMP3->isRunning();
    Serial.printf("  musicMP3->isRunning() = %s\n", running ? "true" : "false");
    return true;
  } else {
    Serial.println("ERROR: musicMP3->begin() failed!");
    Serial.println("  This usually means:");
    Serial.println("    1. Invalid MP3 file format");
    Serial.println("    2. File is corrupted");
    Serial.println("    3. Insufficient memory");
    // Don't delete musicMP3 - keep it alive for next song
    delete musicFile;
    musicFile = nullptr;
    return false;
  }
}

void stopMusicPlayback() {
  Serial.println("Stopping music playback...");

  if (musicMP3 && musicMP3->isRunning()) {
    Serial.println("Stopping MP3 decoder...");
    musicMP3->stop();
  }

  // CRITICAL FIX: Delete and recreate decoder to prevent stale state
  // This fixes the "second song causes freeze" issue
  if (musicMP3) {
    delete musicMP3;
    musicMP3 = nullptr;
    Serial.println("Deleted MP3 decoder - will recreate fresh on next play");
  }

  // Close file
  if (musicFile) {
    Serial.println("Closing music file...");
    delete musicFile;
    musicFile = nullptr;
    Serial.println("Music file closed");
  }

  if (currentSource == AUDIO_SOURCE_MUSIC) {
    currentSource = AUDIO_SOURCE_NONE;
    Serial.println("Audio Manager: Music stopped");
  }

  currentMusicPath = "";

  Serial.printf("Free heap after stop: %d bytes\n", ESP.getFreeHeap());
}

bool isMusicPlaying() {
  return (currentSource == AUDIO_SOURCE_MUSIC && musicMP3 && musicMP3->isRunning());
}

String getCurrentMusicPath() {
  return currentMusicPath;
}

void updateMusicPlayback() {
  // CRITICAL: Check if decoder needs to be deleted from PREVIOUS iteration
  // (we can't delete inside musicMP3->loop() - that's a "delete this" bug!)
  static bool pendingStop = false;
  static unsigned long lastLoopLog = 0;
  static int loopCount = 0;

  if (pendingStop) {
    stopMusicPlayback();
    pendingStop = false;
    return;
  }

  if (currentSource == AUDIO_SOURCE_MUSIC && musicMP3 && musicMP3->isRunning()) {
    // Feed watchdog before MP3 decoding
    yield();

    // Decode next audio frame (non-blocking, processes one MP3 frame)
    if (!musicMP3->loop()) {
      // Track finished - DON'T delete immediately (we're inside musicMP3->loop()!)
      // Mark for deletion on next iteration
      Serial.println("Track finished - marking for cleanup on next loop");
      pendingStop = true;
    }

    // Feed watchdog after decoding
    yield();
  }
}

// Radio player implementation
bool startRadioStream(const char* url) {
  // Stop any existing audio first
  stopMusicPlayback();
  stopRadioStream();

  delay(150); // Critical: ensure previous stream fully cleaned up

  initAudioManager();

  radioStream = new AudioFileSourceHTTPStream(url);
  if (!radioStream) {
    return false;
  }

  radioMP3 = new AudioGeneratorMP3();
  if (!radioMP3) {
    delete radioStream;
    radioStream = nullptr;
    return false;
  }

  if (radioMP3->begin(radioStream, sharedAudioOutput)) {
    currentSource = AUDIO_SOURCE_RADIO;
    return true;
  } else {
    delete radioMP3;
    delete radioStream;
    radioMP3 = nullptr;
    radioStream = nullptr;
    return false;
  }
}

void stopRadioStream() {
  if (radioMP3) {
    if (radioMP3->isRunning()) {
      radioMP3->stop();
    }
    delay(50);
    delete radioMP3;
    radioMP3 = nullptr;
  }

  if (radioStream) {
    radioStream->close();
    delay(100);
    delete radioStream;
    radioStream = nullptr;
  }

  if (currentSource == AUDIO_SOURCE_RADIO) {
    currentSource = AUDIO_SOURCE_NONE;

    // Don't restart M5Speaker here - it will be started on-demand by safeBeep() when needed
    // This prevents stealing I2S port 0 from subsequent audio playback
    Serial.println("Audio Manager: Radio stopped, I2S port 0 released");
  }

  delay(100); // Extra delay to ensure resources released
}

bool isRadioStreaming() {
  return (currentSource == AUDIO_SOURCE_RADIO && radioMP3 && radioMP3->isRunning());
}

void updateRadioPlayback() {
  if (currentSource == AUDIO_SOURCE_RADIO && radioMP3 && radioMP3->isRunning()) {
    if (!radioMP3->loop()) {
      // Stream error
      stopRadioStream();
    }
  }
}
