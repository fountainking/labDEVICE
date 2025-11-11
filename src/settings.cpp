#include "settings.h"
#include "ui.h"
#include "config.h"
#include "ota_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern Preferences preferences;

// Global settings instance
SystemSettings settings = {
  .soundEnabled = false,
  .dimMode = false,
  .deviceName = "Change this in Settings",
  .timezoneString = "PST8PDT,M3.2.0,M11.1.0",  // PST with DST
  .timezoneAuto = true,  // Auto-detect by default
  .theme = 0,
  .lastKnownTime = 0,
  .findabilityEnabled = true  // Allow finding by default
};

SettingsMenuState settingsState = SETTINGS_MAIN;
int settingsMenuIndex = 0;
int timezoneSelectIndex = 0; // Auto-detect default

// Timezone options with DST support (POSIX format)
const TimezoneOption timezones[] = {
  {"Auto-detect", "AUTO"},
  {"Hawaii", "HST10"},
  {"Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
  {"Pacific (LA, Seattle)", "PST8PDT,M3.2.0,M11.1.0"},
  {"Mountain (Denver)", "MST7MDT,M3.2.0,M11.1.0"},
  {"Central (Chicago)", "CST6CDT,M3.2.0,M11.1.0"},
  {"Eastern (NY)", "EST5EDT,M3.2.0,M11.1.0"},
  {"London", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Paris/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Moscow", "MSK-3"},
  {"Dubai", "GST-4"},
  {"India", "IST-5:30"},
  {"Bangkok", "ICT-7"},
  {"Singapore", "SGT-8"},
  {"Tokyo", "JST-9"},
  {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"}
};

const int timezoneCount = 17;

void autoDetectTimezone() {
  if (WiFi.status() != WL_CONNECTED) {
    return; // Can't auto-detect without WiFi
  }

  HTTPClient http;
  http.begin("http://ip-api.com/json/?fields=timezone");
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String detectedTz = doc["timezone"].as<String>();

      // Map IANA timezone to POSIX string
      if (detectedTz.indexOf("Los_Angeles") >= 0 || detectedTz.indexOf("Seattle") >= 0) {
        settings.timezoneString = "PST8PDT,M3.2.0,M11.1.0";
      } else if (detectedTz.indexOf("Denver") >= 0) {
        settings.timezoneString = "MST7MDT,M3.2.0,M11.1.0";
      } else if (detectedTz.indexOf("Chicago") >= 0) {
        settings.timezoneString = "CST6CDT,M3.2.0,M11.1.0";
      } else if (detectedTz.indexOf("New_York") >= 0) {
        settings.timezoneString = "EST5EDT,M3.2.0,M11.1.0";
      } else if (detectedTz.indexOf("Honolulu") >= 0) {
        settings.timezoneString = "HST10";
      } else if (detectedTz.indexOf("Anchorage") >= 0) {
        settings.timezoneString = "AKST9AKDT,M3.2.0,M11.1.0";
      } else if (detectedTz.indexOf("London") >= 0) {
        settings.timezoneString = "GMT0BST,M3.5.0/1,M10.5.0";
      } else if (detectedTz.indexOf("Paris") >= 0 || detectedTz.indexOf("Berlin") >= 0) {
        settings.timezoneString = "CET-1CEST,M3.5.0,M10.5.0/3";
      } else if (detectedTz.indexOf("Moscow") >= 0) {
        settings.timezoneString = "MSK-3";
      } else if (detectedTz.indexOf("Dubai") >= 0) {
        settings.timezoneString = "GST-4";
      } else if (detectedTz.indexOf("Kolkata") >= 0 || detectedTz.indexOf("Mumbai") >= 0) {
        settings.timezoneString = "IST-5:30";
      } else if (detectedTz.indexOf("Bangkok") >= 0) {
        settings.timezoneString = "ICT-7";
      } else if (detectedTz.indexOf("Singapore") >= 0) {
        settings.timezoneString = "SGT-8";
      } else if (detectedTz.indexOf("Tokyo") >= 0) {
        settings.timezoneString = "JST-9";
      } else if (detectedTz.indexOf("Sydney") >= 0) {
        settings.timezoneString = "AEST-10AEDT,M10.1.0,M4.1.0/3";
      } else if (detectedTz.indexOf("Auckland") >= 0) {
        settings.timezoneString = "NZST-12NZDT,M9.5.0,M4.1.0/3";
      }

      applyTimezone();
    }
  }
  http.end();
}

void applyTimezone() {
  // Use configTzTime which handles DST automatically
  configTzTime(settings.timezoneString.c_str(), "pool.ntp.org", "time.nist.gov");
}

void loadSettings() {
  preferences.begin("settings", true); // Read-only
  settings.soundEnabled = preferences.getBool("sound", false);
  settings.dimMode = preferences.getBool("dim", false);
  settings.deviceName = preferences.getString("deviceName", "Change this in Settings");
  settings.timezoneString = preferences.getString("tzString", "PST8PDT,M3.2.0,M11.1.0");
  settings.timezoneAuto = preferences.getBool("tzAuto", true);
  settings.theme = preferences.getInt("theme", 0);
  settings.lastKnownTime = preferences.getULong("lastTime", 0);
  settings.findabilityEnabled = preferences.getBool("findable", true);
  preferences.end();

  // If we have a saved time and no internet, use saved time
  if (settings.lastKnownTime > 0) {
    struct timeval tv;
    tv.tv_sec = settings.lastKnownTime;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
  }

  // Apply settings
  applyBrightness();

  // Apply sound setting - use volume control instead of end()
  if (!settings.soundEnabled) {
    M5Cardputer.Speaker.setVolume(0); // Mute speaker if sound is off
  } else {
    M5Cardputer.Speaker.setVolume(40); // Set lower volume if sound is on
  }

  // Find timezone index
  for (int i = 0; i < timezoneCount; i++) {
    if (String(timezones[i].tzString) == settings.timezoneString ||
        (settings.timezoneAuto && String(timezones[i].tzString) == "AUTO")) {
      timezoneSelectIndex = i;
      break;
    }
  }
}

void saveSettings() {
  preferences.begin("settings", false); // Read-write
  preferences.putBool("sound", settings.soundEnabled);
  preferences.putBool("dim", settings.dimMode);
  preferences.putString("deviceName", settings.deviceName);
  preferences.putString("tzString", settings.timezoneString);
  preferences.putBool("tzAuto", settings.timezoneAuto);
  preferences.putInt("theme", settings.theme);
  preferences.putBool("findable", settings.findabilityEnabled);
  preferences.end();
}

void applyBrightness() {
  if (settings.dimMode) {
    M5Cardputer.Display.setBrightness(50);  // Dim
  } else {
    M5Cardputer.Display.setBrightness(255); // Full brightness
  }
}

void toggleSound() {
  settings.soundEnabled = !settings.soundEnabled;
  saveSettings();

  // Control speaker volume based on setting
  if (settings.soundEnabled) {
    M5Cardputer.Speaker.setVolume(40); // Lower volume
    delay(50); // Give speaker time to be ready
    M5Cardputer.Speaker.tone(1000, 150); // Play confirmation sound
    delay(150);
  } else {
    M5Cardputer.Speaker.setVolume(0); // Mute
  }
}

void toggleDimMode() {
  settings.dimMode = !settings.dimMode;
  saveSettings();
  applyBrightness();

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(800, 50);
  }
}

void toggleFindability() {
  settings.findabilityEnabled = !settings.findabilityEnabled;
  saveSettings();

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(settings.findabilityEnabled ? 1200 : 600, 50);
  }
}

void enterSettingsApp() {
  settingsState = SETTINGS_MAIN;
  settingsMenuIndex = 0;
  drawSettingsMenu();
}

void drawSettingsMenu() {
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.drawString("Settings", 70, 30);

  const char* menuItems[] = {
    "Sound: ",
    "Brightness: ",
    "Device Name: ",
    "Timezone: ",
    "Friend Compass",
    "Findability: ",
    "Theme: ",
    "OTA Update"
  };

  // Draw menu items
  for (int i = 0; i < 8; i++) {
    int yPos = 50 + (i * 12);

    if (i == settingsMenuIndex) {
      M5Cardputer.Display.fillRoundRect(5, yPos - 2, 230, 12, 5, TFT_LIGHTGREY);
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLACK);
    M5Cardputer.Display.drawString(menuItems[i], 15, yPos);

    // Draw current values
    M5Cardputer.Display.setTextColor(TFT_BLUE);
    String value = "";
    int valueX = 100;

    switch(i) {
      case 0: // Sound
        value = settings.soundEnabled ? "ON" : "OFF";
        break;
      case 1: // Brightness
        value = settings.dimMode ? "DIM" : "BRIGHT";
        break;
      case 2: // Device Name
        value = settings.deviceName;
        if (value.length() > 15) value = value.substring(0, 15);
        valueX = 90;
        break;
      case 3: // Timezone
        value = String(timezones[timezoneSelectIndex].name);
        if (value.length() > 20) value = value.substring(0, 20);
        valueX = 90;
        break;
      case 4: // Friend Compass
        value = ""; // No value, just launches
        break;
      case 5: // Findability
        value = settings.findabilityEnabled ? "ON" : "OFF";
        break;
      case 6: // Theme
        value = "Coming Soon";
        valueX = 80;
        break;
      case 7: // OTA Update
        value = FIRMWARE_VERSION;
        valueX = 90;
        break;
    }

    if (!value.isEmpty()) {
      M5Cardputer.Display.drawString(value.c_str(), valueX, yPos);
    }
  }
}

void drawTimezoneSelector() {
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.drawString("Select Timezone", 40, 30);

  // Show 5 timezones at a time, centered on selection
  int startIdx = max(0, timezoneSelectIndex - 2);
  int endIdx = min(timezoneCount, startIdx + 5);

  // Adjust start if we're at the end
  if (endIdx == timezoneCount && timezoneCount >= 5) {
    startIdx = max(0, timezoneCount - 5);
  }

  for (int i = startIdx; i < endIdx; i++) {
    int yPos = 50 + ((i - startIdx) * 15);

    if (i == timezoneSelectIndex) {
      M5Cardputer.Display.fillRoundRect(5, yPos - 2, 230, 14, 5, TFT_LIGHTGREY);
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLACK);

    String tzName = String(timezones[i].name);
    if (tzName.length() > 34) {
      tzName = tzName.substring(0, 34) + "...";
    }
    M5Cardputer.Display.drawString(tzName.c_str(), 10, yPos);
  }
}

void drawThemePlaceholder() {
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.drawString("Themes", 80, 50);

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_BLUE);
  M5Cardputer.Display.drawString("Coming Soon!", 75, 75);

  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.drawString("Press ` to go back", 60, 110);
}

void handleSettingsNavigation(char key) {
  if (settingsState == SETTINGS_MAIN) {
    if (key == ',' || key == ';') {
      if (settingsMenuIndex > 0) {
        settingsMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 30);
        drawSettingsMenu();
      }
    } else if (key == '/' || key == '.') {
      if (settingsMenuIndex < 7) {
        settingsMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 30);
        drawSettingsMenu();
      }
    }
  } else if (settingsState == SETTINGS_TIMEZONE) {
    if (key == ',' || key == ';') {
      if (timezoneSelectIndex > 0) {
        timezoneSelectIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(600, 30);
        drawTimezoneSelector();
      }
    } else if (key == '/' || key == '.') {
      if (timezoneSelectIndex < timezoneCount - 1) {
        timezoneSelectIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 30);
        drawTimezoneSelector();
      }
    }
  }
}

void drawDeviceNameInput() {
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  M5Cardputer.Display.drawString("Device Name", 60, 30);

  // Draw rounded rectangle input box
  M5Cardputer.Display.drawRoundRect(20, 55, 200, 20, 5, TFT_BLUE);

  // Show current name centered in box
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_BLACK);
  String displayName = settings.deviceName;
  if (displayName.length() > 30) {
    displayName = displayName.substring(displayName.length() - 30);
  }

  // Center text in the box
  int textWidth = displayName.length() * 6;
  int textX = 120 - (textWidth / 2);  // Center horizontally
  M5Cardputer.Display.drawString(displayName.c_str(), textX, 60);

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(TFT_DARKGREY);
  M5Cardputer.Display.drawString("Type name, Enter=Save `=Cancel", 15, 110);
}
