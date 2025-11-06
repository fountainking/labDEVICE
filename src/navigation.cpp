#include "navigation.h"
#include "ui.h"
#include "wifi_manager.h"
#include "wifi_fun.h"
#include "file_manager.h"
#include "wifi_transfer.h"
#include "settings.h"
#include "terminal.h"
#include "star_rain.h"
#include "roadmap.h"
#include "the_book.h"
#include "radio.h"
#include "music_player.h"
#include "music_tools.h"
#include "embedded_gifs.h"
#include <AnimatedGIF.h>

// GIF objects for star animations (using embedded GIFs from PROGMEM)
AnimatedGIF starGif;
bool starGifOpen = false;
bool starGifPlaying = false;
const unsigned char* starGifData = nullptr;
unsigned int starGifDataLen = 0;
int starGifFrameCount = 0;
int starGifXOffset = 0;
int starGifYOffset = 0;

// Custom GIF draw function for star animations (positioned at bottom center)
// 50% scaling - draws 32x32 GIFs at 16x16 (skip every other pixel)
void StarGIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > 240) iWidth = 240;

  usPalette = pDraw->pPalette;

  // Apply custom offset for star positioning with 50% scaling
  // Only draw every other line (skip odd lines for 50% vertical scale)
  if ((pDraw->iY + pDraw->y) % 2 != 0) return;

  x = pDraw->iX / 2 + starGifXOffset;
  y = (pDraw->iY + pDraw->y) / 2 + starGifYOffset;

  if (y >= 135 || x >= 240) return; // Off screen

  s = pDraw->pPixels;

  // Handle transparency with 50% horizontal scaling (skip every other pixel)
  if (pDraw->ucHasTransparency) {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    for (int px = 0; px < iWidth; px += 2) { // Skip every other pixel
      if (s[px] == ucTransparent) {
        continue;
      }
      c = s[px];
      M5Cardputer.Display.drawPixel(x + (px / 2), y, usPalette[c]);
    }
  } else {
    // No transparency - draw every other pixel
    for (int px = 0; px < iWidth; px += 2) {
      M5Cardputer.Display.drawPixel(x + (px / 2), y, usPalette[s[px]]);
    }
  }
}

void drawStillStar() {
  // Initialize GIF with Little Endian palette
  starGif.begin(GIF_PALETTE_RGB565_LE);

  // Use left star as default still image (from embedded PROGMEM)
  if (starGif.open((uint8_t*)gif_star_left, gif_star_left_len, StarGIFDraw)) {
    // Get GIF dimensions for centering
    int gifWidth = starGif.getCanvasWidth();
    int gifHeight = starGif.getCanvasHeight();

    // 50% scaling - 32x32 GIF displays as 16x16
    // Center at position 120, 118
    starGifXOffset = 120 - (gifWidth / 4);
    starGifYOffset = 118 - (gifHeight / 4);

    // Draw only first frame (still image)
    starGif.playFrame(false, NULL);
    starGif.close();
  }
}

void startStarGif(bool isLeft) {
  // Set which embedded GIF to play based on direction
  if (isLeft) {
    starGifData = gif_star_left;
    starGifDataLen = gif_star_left_len;
  } else {
    starGifData = gif_star_right;
    starGifDataLen = gif_star_right_len;
  }
}

void updateStarGifPlayback() {
  // Check if we need to start a new animation (can interrupt current one)
  if (starGifData != nullptr && starGifDataLen > 0) {
    // Stop any currently playing GIF to allow restart
    if (starGifOpen) {
      starGif.close();
      starGifOpen = false;
      starGifPlaying = false;
    }

    // Initialize GIF with Little Endian palette
    starGif.begin(GIF_PALETTE_RGB565_LE);

    // Try to open the embedded GIF from PROGMEM
    if (starGif.open((uint8_t*)starGifData, starGifDataLen, StarGIFDraw)) {
      starGifOpen = true;
      starGifPlaying = true;
      starGifFrameCount = 0;

      // Get GIF dimensions for centering
      int gifWidth = starGif.getCanvasWidth();
      int gifHeight = starGif.getCanvasHeight();

      // 50% scaling - 32x32 GIF displays as 16x16
      starGifXOffset = 120 - (gifWidth / 4);
      starGifYOffset = 118 - (gifHeight / 4);

      // Clear the request
      starGifData = nullptr;
      starGifDataLen = 0;
    }
  }

  // Play one frame per call for non-blocking, interruptible animation
  if (starGifPlaying && starGifOpen) {
    // Background color matches current theme (white or black)
    extern bool uiInverted;
    uint16_t bgColor = uiInverted ? TFT_BLACK : TFT_WHITE;

    // Clear star area before drawing next frame to prevent layering
    M5Cardputer.Display.fillRect(starGifXOffset, starGifYOffset, 18, 18, bgColor);

    if (!starGif.playFrame(true, NULL)) {
      // Animation finished
      Serial.printf("Animation complete after %d frames\n", starGifFrameCount);
      starGif.close();
      starGifOpen = false;
      starGifPlaying = false;
      drawStillStar();
      return;
    }

    starGifFrameCount++;

    // Safety limit
    if (starGifFrameCount > 50) {
      Serial.println(F("Hit safety limit - stopping animation"));
      starGif.close();
      starGifOpen = false;
      starGifPlaying = false;
      drawStillStar();
    }
  }
}

void navigateUp() {
  // Same as navigateLeft for most screens
  navigateLeft();
}

void navigateLeft() {
  extern bool uiInverted;  // Use global inversion state
  bool inverted = uiInverted;

  if (currentState == WIFI_SAVED) {
    if (selectedSavedIndex > 0) {
      selectedSavedIndex--;
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
      drawWiFiSaved();
    }
  } else if (currentState == WIFI_SCAN) {
    if (selectedNetworkIndex > 0) {
      selectedNetworkIndex--;
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
      drawWiFiScan();
    }
  } else if (currentState == APPS_MENU) {
    if (currentAppIndex > 0) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
      // INSTANT RESPONSE: Change state and draw new screen IMMEDIATELY
      currentAppIndex--;
      drawScreen(inverted);
      // THEN start star animation (plays on top of already-drawn screen)
      startStarGif(true);  // true = left
      updateStarGifPlayback();
    } else {
      // At first app, go back to screensaver with star rain
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
      extern bool screensaverActive;
      initStarRain(STARRAIN_SCREENSAVER);
      screensaverActive = true;
    }
  } else if (currentState == MAIN_MENU) {
    if (currentMainIndex > 0) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
      // INSTANT RESPONSE: Change state and draw new screen IMMEDIATELY
      currentMainIndex--;
      drawScreen(inverted);
      // THEN start star animation (plays on top of already-drawn screen)
      startStarGif(true);  // true = left
      updateStarGifPlayback();
    }
    // At first item - do nothing (no landing page)
  }
}

void navigateRight() {
  extern bool uiInverted;  // Use global inversion state
  bool inverted = uiInverted;
  
  if (currentState == WIFI_SAVED) {
    if (selectedSavedIndex < numSavedNetworks - 1) {
      selectedSavedIndex++;
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
      drawWiFiSaved();
    }
  } else if (currentState == WIFI_SCAN) {
    int totalItems = min(numSavedNetworks, 5) + numNetworks;
    if (selectedNetworkIndex < totalItems - 1) {
      selectedNetworkIndex++;
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
      drawWiFiScan();
    }
  } else if (currentState == APPS_MENU) {
    if (currentAppIndex < totalApps - 1) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
      // INSTANT RESPONSE: Change state and draw new screen IMMEDIATELY
      currentAppIndex++;
      drawScreen(inverted);
      // THEN start star animation (plays on top of already-drawn screen)
      startStarGif(false);  // false = right
      updateStarGifPlayback();
    }
  } else if (currentState == MAIN_MENU) {
    if (currentMainIndex < totalMainItems - 1) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
      // INSTANT RESPONSE: Change state and draw new screen IMMEDIATELY
      currentMainIndex++;
      drawScreen(inverted);
      // THEN start star animation (plays on top of already-drawn screen)
      startStarGif(false);  // false = right
      updateStarGifPlayback();
    }
  }
}

void handleSelect() {
  extern bool uiInverted;  // Use global inversion state

  if (currentState == MAIN_MENU) {
    if (currentMainIndex == 0) {
      // APPS
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = APPS_MENU;
      currentAppIndex = 0;
      drawScreen(uiInverted);
    } else if (currentMainIndex == 1) {
      // Join Wi-Fi
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      scanWiFiNetworks();
    } else if (currentMainIndex == 2) {
      // The Book
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = SCREEN_VIEW;
      currentScreenNumber = 7;
      enterTheBook();
    } else if (currentMainIndex == 3) {
      // Games
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = SCREEN_VIEW;
      currentScreenNumber = 8;
      drawGamesMenu();
    } else if (currentMainIndex == 4) {
      // LabCHAT
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = SCREEN_VIEW;
      currentScreenNumber = 16;
      extern void enterLabChat();
      enterLabChat();
    } else if (currentMainIndex == 5) {
      // Settings
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = SCREEN_VIEW;
      currentScreenNumber = 11;
      enterSettingsApp();
    } else if (currentMainIndex == 6) {
      // About
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      currentState = SCREEN_VIEW;
      currentScreenNumber = 9;
      enterRoadmap();
    }
  } else if (currentState == APPS_MENU) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
    currentState = SCREEN_VIEW;
    currentScreenNumber = apps[currentAppIndex].screenNumber;
    
    if (currentScreenNumber == 1) {
      enterFileManager();
    } else if (currentScreenNumber == 2) {
      setCpuFrequencyMhz(240); // WiFi operations need 240MHz
      Serial.println("CPU: 240MHz (WiFi Fun)");
      enterWiFiFunApp();
    } else if (currentScreenNumber == 3) {
      setCpuFrequencyMhz(240); // WiFi transfer needs 240MHz
      Serial.println("CPU: 240MHz (Transfer)");
      enterWiFiTransferApp();
    } else if (currentScreenNumber == 4) {
      enterRadioApp();
    } else if (currentScreenNumber == 5) {
      enterTerminal();
    } else if (currentScreenNumber == 6) {
      setCpuFrequencyMhz(240); // Audio needs 240MHz
      Serial.println("CPU: 240MHz (Music)");
      enterMusicPlayer();
    } else if (currentScreenNumber == 7) {
      enterTheBook();
    } else if (currentScreenNumber == 12) {
      // Music menu
      extern void drawMusicMenu();
      extern int musicMenuIndex;
      musicMenuIndex = 0;
      drawMusicMenu();
    } else {
      drawPlaceholderScreen(currentScreenNumber, apps[currentAppIndex].name.c_str(), true);
    }
  } else if (currentState == WIFI_SCAN) {
    int numSaved = min(numSavedNetworks, 5);
    
    if (selectedNetworkIndex < numSaved) {
      // Selecting a saved network - connect directly
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      connectToSavedNetwork(selectedNetworkIndex);
    } else {
      // Selecting a scanned network
      int scannedIndex = selectedNetworkIndex - numSaved;
      if (scannedIndex < numNetworks) {
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
        targetSSID = scannedNetworks[scannedIndex];
        
        // Check if already saved
        bool isSaved = false;
        int savedIdx = -1;
        for (int i = 0; i < numSavedNetworks; i++) {
          if (savedSSIDs[i] == targetSSID) {
            isSaved = true;
            savedIdx = i;
            break;
          }
        }
        
        if (isSaved) {
          connectToSavedNetwork(savedIdx);
        } else {
          inputPassword = "";
          currentState = WIFI_PASSWORD;
          drawWiFiPassword();
        }
      }
    }
  } else if (currentState == WIFI_PASSWORD) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
    connectToWiFi();
  } else if (currentState == WIFI_SAVED) {
    if (numSavedNetworks > 0) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
      connectToSavedNetwork(selectedSavedIndex);
    }
  }
}

void handleBack() {
  extern bool uiInverted;  // Use global inversion state

  // ALWAYS reset speaker volume first to prevent loud beeps
  M5Cardputer.Speaker.setVolume(40);

  if (currentState == SCREEN_VIEW) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);

    if (currentScreenNumber == 2 && wifiFunState != WIFI_FUN_MENU) {
      return;
    }

    if (currentScreenNumber == 1 && sdCardMounted) {
      return;
    }

    // Route back based on screen number:
    // APPS menu: 1 (Files), 2 (WiFi Fun), 3 (Transfer), 5 (Terminal), 12 (Music menu)
    // MAIN menu: 7 (The Book), 8 (Games), 9 (About), 10 (Join WiFi), 11 (Settings)
    // Music menu: 4 (Radio), 6 (Music Player), 13 (Music Tools)

    if (currentScreenNumber == 1 || currentScreenNumber == 2 || currentScreenNumber == 3 ||
        currentScreenNumber == 5 || currentScreenNumber == 12) {
      // APPS menu screens
      currentState = APPS_MENU;
      drawScreen(uiInverted);
    } else if (currentScreenNumber == 4 || currentScreenNumber == 6 || currentScreenNumber == 13) {
      // Music submenu screens - go back to Music menu
      int prevScreen = currentScreenNumber;  // Save before changing
      currentScreenNumber = 12;
      extern int musicMenuIndex;
      if (prevScreen == 4) musicMenuIndex = 1;  // Radio
      else if (prevScreen == 6) musicMenuIndex = 0;  // Player
      else musicMenuIndex = 2;  // Tools
      extern void drawMusicMenu();
      drawMusicMenu();
    } else {
      // MAIN menu screens (7, 8, 9, 10, 11)
      currentState = MAIN_MENU;
      drawScreen(uiInverted);
    }
  } else if (currentState == APPS_MENU) {
    // ESC from page 2 (APPS) - go back to page 1 (MAIN)
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);
    currentState = MAIN_MENU;
    drawScreen(uiInverted);
  } else if (currentState == MAIN_MENU) {
    // ESC from page 1 (MAIN with APPS option) - go to APPS window (page 2)
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);
    currentState = APPS_MENU;
    drawScreen(uiInverted);
  } else if (currentState == WIFI_SCAN) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);
    currentState = MAIN_MENU;
    drawScreen(uiInverted);
  } else if (currentState == WIFI_PASSWORD) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);
    inputPassword = "";
    currentState = WIFI_SCAN;
    drawWiFiScan();
  } else if (currentState == WIFI_SAVED) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 100);
    returnToMainMenu();
  }
}

// Helper function to return to main menu with CPU scaling
void returnToMainMenu() {
  // Drop CPU to 80MHz for battery savings
  setCpuFrequencyMhz(80);
  Serial.println("CPU: 80MHz (idle)");

  currentState = MAIN_MENU;
  extern bool uiInverted;
  drawScreen(uiInverted);
}