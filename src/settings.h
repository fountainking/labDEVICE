#ifndef SETTINGS_H
#define SETTINGS_H

#include <M5Cardputer.h>
#include <Preferences.h>

// Settings state
enum SettingsMenuState {
  SETTINGS_MAIN,
  SETTINGS_DEVICE_NAME,
  SETTINGS_TIMEZONE,
  SETTINGS_THEME
};

// Settings structure
struct SystemSettings {
  bool soundEnabled;
  bool dimMode;
  String deviceName;      // User-defined device name
  String timezoneString;  // POSIX timezone string (e.g., "PST8PDT,M3.2.0,M11.1.0")
  bool timezoneAuto;      // Auto-detect timezone from IP
  int theme;              // 0 = default, 1-n = future themes
  time_t lastKnownTime;   // Last synced time for offline use
  bool findabilityEnabled; // Allow others to find you via Friend Compass
};

// Global settings
extern SystemSettings settings;
extern SettingsMenuState settingsState;
extern int settingsMenuIndex;
extern int timezoneSelectIndex;

// Timezone options
struct TimezoneOption {
  const char* name;
  const char* tzString;  // POSIX timezone string
};

extern const TimezoneOption timezones[];
extern const int timezoneCount;

// Functions
void enterSettingsApp();
void loadSettings();
void saveSettings();
void drawSettingsMenu();
void drawDeviceNameInput();
void drawTimezoneSelector();
void drawThemePlaceholder();
void handleSettingsNavigation(char key);
void toggleSound();
void toggleDimMode();
void toggleFindability();
void applyBrightness();
void autoDetectTimezone();
void applyTimezone();

#endif
