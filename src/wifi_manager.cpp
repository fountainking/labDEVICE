#include "wifi_manager.h"
#include "ui.h"
#include "settings.h"
#include <WiFi.h>
#include <Preferences.h>

extern Preferences preferences;

void scanWiFiNetworks() {
  currentState = WIFI_SCAN;
  numNetworks = 0;
  selectedNetworkIndex = 0;

  // Boost CPU for WiFi operations (240MHz)
  setCpuFrequencyMhz(240);
  Serial.println("CPU: 240MHz (WiFi scan)");

  // Enable WiFi for scanning
  WiFi.mode(WIFI_STA);
  delay(100);

  // Disconnect from any active connection before scanning
  WiFi.disconnect();
  delay(100);

  drawWiFiScan(); // This will show "Scanning networks..." message

  int n = WiFi.scanNetworks(); // BLOCKING - takes 1-3 seconds
  numNetworks = min(n, 20);
  
  for (int i = 0; i < numNetworks; i++) {
    scannedNetworks[i] = WiFi.SSID(i);
    scannedRSSI[i] = WiFi.RSSI(i);
  }
  
  // Sort by signal strength
  for (int i = 0; i < numNetworks - 1; i++) {
    for (int j = i + 1; j < numNetworks; j++) {
      if (scannedRSSI[j] > scannedRSSI[i]) {
        String tempSSID = scannedNetworks[i];
        int tempRSSI = scannedRSSI[i];
        scannedNetworks[i] = scannedNetworks[j];
        scannedRSSI[i] = scannedRSSI[j];
        scannedNetworks[j] = tempSSID;
        scannedRSSI[j] = tempRSSI;
      }
    }
  }
  
  drawWiFiScan();
}

void connectToWiFi() {
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);
  
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLUE);
  M5Cardputer.Display.drawString("Connecting...", 50, 60);
  
  WiFi.begin(targetSSID.c_str(), inputPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    M5Cardputer.Display.fillCircle(100 + (attempts * 10), 90, 3, TFT_BLUE);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawStatusBar(false);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.drawString("Connected!", 60, 60);
    // M5Cardputer.Speaker.tone(1200, 100);
    
    // Sync time with NTP using timezone with DST support
    applyTimezone();

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLUE);
    M5Cardputer.Display.drawString("Syncing time...", 70, 85);

    // Wait for time sync with reduced timeout (max 1 second)
    int syncAttempts = 0;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) && syncAttempts < 2) {
      delay(500);
      syncAttempts++;
    }

    if (syncAttempts < 2) {
      timeIsSynced = true;
      lastSyncTime = millis();

      // Save synced time to preferences
      time_t now = time(nullptr);
      settings.lastKnownTime = now;
      preferences.begin("settings", false);
      preferences.putULong("lastTime", (unsigned long)now);
      preferences.end();

      M5Cardputer.Display.setTextColor(TFT_GREEN);
      M5Cardputer.Display.drawString("Time synced!", 75, 100);
      delay(300);
    } else {
      M5Cardputer.Display.setTextColor(TFT_ORANGE);
      M5Cardputer.Display.drawString("Time sync timeout", 65, 100);
      delay(300);
    }
    
    delay(500);

    // Save network if connection successful
    saveNetwork(targetSSID, inputPassword);
  } else {
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawStatusBar(false);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.drawString("Failed!", 70, 60);
    // M5Cardputer.Speaker.tone(400, 200);
    delay(1500);

    // Turn off WiFi if connection failed (save ~80mA)
    WiFi.mode(WIFI_OFF);
  }

  currentState = MAIN_MENU;
  inputPassword = "";
  drawScreen(false);
}

void saveNetwork(String ssid, String password) {
  // Check if network already exists
  for (int i = 0; i < numSavedNetworks; i++) {
    if (savedSSIDs[i] == ssid) {
      // Update password if SSID exists
      savedPasswords[i] = password;
      preferences.begin("wifi", false);
      preferences.putString(("pass" + String(i)).c_str(), password);
      preferences.end();
      return;
    }
  }
  
  // Add new network if space available
  if (numSavedNetworks < MAX_SAVED_NETWORKS) {
    savedSSIDs[numSavedNetworks] = ssid;
    savedPasswords[numSavedNetworks] = password;
    
    preferences.begin("wifi", false);
    preferences.putString(("ssid" + String(numSavedNetworks)).c_str(), ssid);
    preferences.putString(("pass" + String(numSavedNetworks)).c_str(), password);
    preferences.putInt("count", numSavedNetworks + 1);
    preferences.end();
    
    numSavedNetworks++;
  }
}

void deleteNetwork(int index) {
  if (index < 0 || index >= numSavedNetworks) return;
  
  // Shift networks down
  for (int i = index; i < numSavedNetworks - 1; i++) {
    savedSSIDs[i] = savedSSIDs[i + 1];
    savedPasswords[i] = savedPasswords[i + 1];
  }
  
  numSavedNetworks--;
  
  // Save to preferences
  preferences.begin("wifi", false);
  preferences.putInt("count", numSavedNetworks);
  for (int i = 0; i < numSavedNetworks; i++) {
    preferences.putString(("ssid" + String(i)).c_str(), savedSSIDs[i]);
    preferences.putString(("pass" + String(i)).c_str(), savedPasswords[i]);
  }
  // Clear the last slot
  preferences.remove(("ssid" + String(numSavedNetworks)).c_str());
  preferences.remove(("pass" + String(numSavedNetworks)).c_str());
  preferences.end();
}

void connectToSavedNetwork(int index) {
  if (index < 0 || index >= numSavedNetworks) return;
  
  M5Cardputer.Display.fillScreen(TFT_WHITE);
  drawStatusBar(false);
  
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.setTextColor(TFT_BLUE);
  M5Cardputer.Display.drawString("Connecting...", 50, 60);
  
  WiFi.begin(savedSSIDs[index].c_str(), savedPasswords[index].c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    M5Cardputer.Display.fillCircle(100 + (attempts * 10), 90, 3, TFT_BLUE);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawStatusBar(false);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.drawString("Connected!", 60, 60);
    // M5Cardputer.Speaker.tone(1200, 100);
    
    // Sync time with NTP using timezone with DST support
    applyTimezone();

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(TFT_BLUE);
    M5Cardputer.Display.drawString("Syncing time...", 70, 85);

    struct tm timeinfo;
    int syncAttempts = 0;
    while (!getLocalTime(&timeinfo) && syncAttempts < 2) {
      delay(500);
      syncAttempts++;
    }

    if (syncAttempts < 2) {
      timeIsSynced = true;
      lastSyncTime = millis();

      // Save synced time to preferences
      time_t now = time(nullptr);
      settings.lastKnownTime = now;
      preferences.begin("settings", false);
      preferences.putULong("lastTime", (unsigned long)now);
      preferences.end();

      M5Cardputer.Display.setTextColor(TFT_GREEN);
      M5Cardputer.Display.drawString("Time synced!", 75, 100);
      delay(300);
    } else {
      M5Cardputer.Display.setTextColor(TFT_ORANGE);
      M5Cardputer.Display.drawString("Time sync timeout", 65, 100);
      delay(300);
    }
    
    delay(500);
  } else {
    M5Cardputer.Display.fillScreen(TFT_WHITE);
    drawStatusBar(false);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.drawString("Failed!", 70, 60);
    // M5Cardputer.Speaker.tone(400, 200);
    delay(1500);

    // Turn off WiFi if connection failed (save ~80mA)
    WiFi.mode(WIFI_OFF);
  }

  currentState = MAIN_MENU;
  drawScreen(false);
}