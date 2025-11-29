#ifndef UI_H
#define UI_H

#include "config.h"
#include <M5GFX.h>

// Global canvas for double-buffered rendering
extern M5Canvas canvas;

// Theme state (defined in main.cpp)
extern bool uiInverted;

// Theme helper functions - use these instead of hardcoded TFT_WHITE/TFT_BLACK
inline uint16_t getThemeBgColor() {
    return uiInverted ? TFT_BLACK : TFT_WHITE;
}

inline uint16_t getThemeFgColor() {
    return uiInverted ? TFT_WHITE : TFT_BLACK;
}

// Utility functions
String getCurrentTime();
int getBatteryPercent();
bool isCharging();

// Menu animation state
struct MenuAnimation {
    bool active;
    unsigned long startTime;
    int fromIndex;
    int toIndex;
    int direction;  // -1 = left, +1 = right
    bool isAppsMenu;  // true = APPS_MENU, false = MAIN_MENU
};

extern MenuAnimation menuAnim;

// Animation constants
const int MENU_ANIM_DURATION = 180;  // ms
const int CARD_WIDTH = 200;
const int SCREEN_WIDTH = 240;

// Animation functions
void startMenuAnimation(int fromIdx, int toIdx, int dir, bool isApps);
bool updateMenuAnimation();  // Returns true if animation is active
float easeOutBounce(float t);

// Drawing functions
void drawStar(int x, int y, int size, float angle, uint16_t fillColor, uint16_t outlineColor);
void drawIndicatorDots(int currentIndex, int totalItems, bool inverted = false);
void drawStatusBar(bool inverted = false);
void drawCard(const char* label, int xOffset = 0, bool inverted = false);
void drawPlaceholderScreen(int screenNum, const char* title, bool inverted = false);
void drawScreen(bool inverted = false);

// WiFi UI functions
void drawWiFiScan();
void drawWiFiPassword();
void drawWiFiSaved();

// Music UI functions
void drawMusicMenu();

// Games UI functions
void drawGamesMenu();

#endif