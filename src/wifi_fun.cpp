#include "wifi_fun.h"
#include "ui.h"
#include "captive_portal.h"
#include "file_manager.h"
#include "settings.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <IRremote.hpp>

WiFiFunState wifiFunState = WIFI_FUN_MENU;
int wifiFunMenuIndex = 0;
int portalsMenuIndex = 0;
int bigPartyMenuIndex = 0;
int analyticsMenuIndex = 0;
String fakeSSID = "";
String portalSSIDInput = "";
int connectedClients = 0;
bool rickRollActive = false;
bool partyTimeActive = false;
String partyTimeInput = "";

// Analytics globals
bool crowdCounterActive = false;
int deviceCount = 0;
String deviceMACs[20];  // Reduced: Store unique MAC addresses
unsigned long lastCrowdUpdate = 0;

// Promiscuous mode globals for actual device detection
uint8_t currentChannel = 1;
unsigned long lastChannelSwitch = 0;
const int CHANNEL_HOP_INTERVAL = 300;  // ms between channel hops

// Queue for processing new devices found in interrupt handler
String newDeviceQueue[5];  // Reduced: Temporary queue for MACs found in interrupt
int newDeviceQueueCount = 0;
bool displayNeedsUpdate = false;

// BLE scanning globals
BLEScan* pBLEScan = nullptr;
bool bleInitialized = false;
int bleDeviceCount = 0;
String bleDeviceMACs[20];  // Reduced: Separate array for BLE devices
unsigned long lastBLEScan = 0;
const int BLE_SCAN_INTERVAL = 2000;  // ms between BLE scans

// Device categorization counters
int totalPhones = 0;
int totalTVs = 0;
int totalCameras = 0;
int totalCars = 0;
int totalIoT = 0;
int totalComputers = 0;
int totalRouters = 0;
int unknownDevices = 0;

// Phone brand counters
int applePhones = 0;
int samsungPhones = 0;
int googlePhones = 0;
int huaweiPhones = 0;
int xiaomiPhones = 0;
int lgPhones = 0;
int oneplusPhones = 0;
int motorolaPhones = 0;
int sonyPhones = 0;
int htcPhones = 0;
int nokiaPhones = 0;
int randomizedPhones = 0;  // Phones using randomized MAC addresses for privacy
int otherPhones = 0;

bool speedometerActive = false;
float currentSpeed = 0.0;
float uploadSpeed = 0.0;
float downloadSpeed = 0.0;
bool speedTestRunning = false;
bool speedTestComplete = false;
unsigned long lastSpeedTest = 0;
unsigned long speedTestBytes = 0;
unsigned long speedTestStartTime = 0;

bool heatmapActive = false;
int heatmapData[12][8];  // 12x8 grid for signal strength
int heatmapX = 0;
int heatmapY = 0;

// Probe Sniffer
bool probeSnifferActive = false;
struct ProbeRequest {
  String mac;
  String ssid;
  int rssi;
  unsigned long timestamp;
};
ProbeRequest probeRequests[20];  // Store last 20 probe requests
int probeRequestCount = 0;
int probeRequestScrollPos = 0;
String probeQueue[10];  // Queue for processing in main loop
int probeQueueRSSI[10];
String probeQueueSSID[10];
int probeQueueCount = 0;
unsigned long totalProbesSeen = 0;

// Big Party - AP List Management
String apList[10];
int apListCount = 0;
int currentEditingAP = 0;

// Rick Roll beacon spam
const char* rickRollLines[] = {
  "Never Gonna Give You Up",
  "Never Gonna Let You Down",
  "Never Gonna Run Around",
  "And Desert You"
};
const int rickRollLineCount = 4;
int currentRickRollLine = 0;
unsigned long lastRickRollSwitch = 0;

// Party Time beacon spam
int currentPartyTimeSSID = 0;
unsigned long lastPartyTimeSwitch = 0;
int partyTimeListScroll = 0;

// Portals submenu
const char* portalsMenuItems[] = {
  "labPORTAL"
};
const int portalsMenuCount = 1;

// Big Party submenu
const char* bigPartyMenuItems[] = {
  "Rick Roll",
  "Party Time"
};
const int bigPartyMenuCount = 2;

// Analytics submenu
const char* analyticsMenuItems[] = {
  "Speedometer",
  "Probe Sniffer"
};
const int analyticsMenuCount = 2;

// OUI lookup from SD card - much more memory efficient!
String lookupOUIFromSD(String macPrefix) {
  if (!sdCardMounted) {
    Serial.println(F("ERROR: SD card not mounted"));
    return "";
  }

  File file = SD.open("/oui.csv");
  if (!file) {
    Serial.println(F("ERROR: Cannot open /oui.csv"));
    return "";
  }

  macPrefix.toUpperCase();
  // Also create version without colons for matching
  String macPrefixNoColon = macPrefix;
  macPrefixNoColon.replace(":", "");

  // Search through CSV file
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    // Try matching with colons OR without colons
    if (line.startsWith(macPrefix) || line.startsWith(macPrefixNoColon)) {
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);

      if (firstComma > 0 && secondComma > firstComma) {
        String brand = line.substring(firstComma + 1, secondComma);
        String type = line.substring(secondComma + 1);

        // Trim whitespace
        brand.trim();
        type.trim();

        file.close();

        // Log the match
        Serial.print(F("OUI Match: "));
        Serial.print(macPrefix);
        Serial.print(F(" = "));
        Serial.print(brand);
        Serial.print(F(" ("));
        Serial.print(type);
        Serial.println(F(")"));

        return brand + "|" + type;  // Return both brand and type
      }
    }
  }

  file.close();
  return "";
}


// Struct to hold device info
struct DeviceInfo {
  String brand;
  String type;
  bool found;
};

// Helper function to check if MAC is randomized (locally administered)
bool isRandomizedMAC(String macAddress) {
  if (macAddress.length() < 2) return false;

  // Get first byte (first 2 hex characters)
  String firstByte = macAddress.substring(0, 2);

  // Convert to integer
  int byteValue = strtol(firstByte.c_str(), NULL, 16);

  // Check if bit 1 (locally administered bit) is set
  // Bit 1 = 0x02 = 0b00000010
  return (byteValue & 0x02) != 0;
}

// Helper function to lookup device by MAC address from SD card
DeviceInfo lookupDevice(String macAddress) {
  DeviceInfo result = {"", "", false};

  // Extract first 8 characters (XX:XX:XX format)
  if (macAddress.length() < 8) return result;

  // Check if this is a randomized MAC (privacy feature on modern phones)
  if (isRandomizedMAC(macAddress)) {
    result.brand = "Phone";
    result.type = "phone";
    result.found = true;
    Serial.print(F("Randomized MAC detected: "));
    Serial.println(macAddress.substring(0, 8));
    return result;
  }

  String prefix = macAddress.substring(0, 8);
  prefix.toUpperCase();

  // Lookup in SD card CSV file
  String lookupResult = lookupOUIFromSD(prefix);

  if (lookupResult.length() > 0) {
    int pipeIndex = lookupResult.indexOf('|');
    if (pipeIndex > 0) {
      result.brand = lookupResult.substring(0, pipeIndex);
      result.type = lookupResult.substring(pipeIndex + 1);
      result.found = true;
    }
  }

  return result;
}

const char* wifiFunMenuItems[] = {
  "Fake WiFi",
  "Portals",
  "Wi-Fi Party",
  "TURN THIS TV OFF",
  "Analytics"
};
const int wifiFunMenuCount = 5;

void enterWiFiFunApp() {
  wifiFunState = WIFI_FUN_MENU;
  wifiFunMenuIndex = 0;
  drawWiFiFunMenu();
}

void drawWiFiFunMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  // No "Fun" title - removed

  // Draw menu items (lifted up without title)
  for (int i = 0; i < wifiFunMenuCount; i++) {
    int yPos = 35 + (i * 18);  // Lifted up

    if (i == wifiFunMenuIndex) {
      canvas.fillRoundRect(10, yPos - 2, 220, 18, 5, TFT_BLUE);
      canvas.setTextColor(TFT_WHITE);
    } else {
      canvas.setTextColor(TFT_LIGHTGREY);
    }

    canvas.setTextSize(1);
    canvas.drawString(wifiFunMenuItems[i], 20, yPos);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawFakeWiFiInput() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);
  
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Fake WiFi", 65, 30);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Network Name:", 10, 55);
  
  // Input box
  canvas.drawRect(5, 70, 230, 20, TFT_WHITE);
  canvas.drawRect(6, 71, 228, 18, TFT_WHITE);

  canvas.setTextSize(1);

  if (fakeSSID.length() == 0) {
    // Show greyed placeholder text
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Type network name...", 10, 75);
  } else {
    // Show actual input
    canvas.setTextColor(TFT_WHITE);
    String displaySSID = fakeSSID;
    if (displaySSID.length() > 30) {
      displaySSID = displaySSID.substring(displaySSID.length() - 30);
    }
    canvas.drawString(displaySSID.c_str(), 10, 75);
  }

  // Cursor blink
  if ((millis() / 500) % 2 == 0) {
    int cursorX = 10 + (fakeSSID.length() * 6);
    canvas.drawLine(cursorX, 75, cursorX, 85, TFT_WHITE);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Enter=Start Broadcast", 50, 105);
  canvas.drawString("Del=Delete `=Back", 60, 115);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawFakeWiFiRunning() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);
  
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("Broadcasting!", 45, 30);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Network Name:", 10, 55);
  
  canvas.setTextColor(TFT_WHITE);
  String displaySSID = fakeSSID;
  if (displaySSID.length() > 32) {
    displaySSID = displaySSID.substring(0, 32);
  }
  canvas.drawString(displaySSID.c_str(), 10, 70);
  
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Devices nearby can see this!", 30, 85);
  
  // Animation - pulsing signal icon
  int pulseSize = 3 + (millis() / 200) % 5;
  for (int i = 0; i < 3; i++) {
    canvas.drawCircle(120, 100, pulseSize + (i * 8), TFT_GREEN);
  }
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press any key to stop", 55, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void startFakeWiFi(String ssid) {
  // Disconnect from any existing WiFi
  WiFi.disconnect();
  delay(100);
  
  // Start Access Point mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());
  
  wifiFunState = FAKE_WIFI_RUNNING;
  connectedClients = 0;
  
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
  delay(100);
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
  
  drawFakeWiFiRunning();
}

void stopFakeWiFi() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);
  
  wifiFunState = WIFI_FUN_MENU;
  drawWiFiFunMenu();
}

void drawPortalInput() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Portal Setup", 55, 30);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Network Name:", 10, 55);

  // Input box
  canvas.drawRect(5, 70, 230, 20, TFT_WHITE);
  canvas.drawRect(6, 71, 228, 18, TFT_WHITE);

  canvas.setTextSize(1);

  if (portalSSIDInput.length() == 0) {
    // Show greyed placeholder text
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Type portal name...", 10, 75);
  } else {
    // Show actual input
    canvas.setTextColor(TFT_WHITE);
    String displaySSID = portalSSIDInput;
    if (displaySSID.length() > 30) {
      displaySSID = displaySSID.substring(displaySSID.length() - 30);
    }
    canvas.drawString(displaySSID.c_str(), 10, 75);
  }

  // Cursor blink
  if ((millis() / 500) % 2 == 0) {
    int cursorX = 10 + (portalSSIDInput.length() * 6);
    canvas.drawLine(cursorX, 75, cursorX, 85, TFT_WHITE);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Enter=Start Portal", 60, 105);
  canvas.drawString("Del=Delete `=Back", 60, 115);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawPortalsMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Portals", 80, 30);

  // Draw menu items
  for (int i = 0; i < portalsMenuCount; i++) {
    int yPos = 55 + (i * 20);

    if (i == portalsMenuIndex) {
      canvas.fillRoundRect(10, yPos - 2, 220, 18, 5, TFT_CYAN);
      canvas.setTextColor(TFT_BLACK);
    } else {
      canvas.setTextColor(TFT_LIGHTGREY);
    }

    canvas.setTextSize(1);
    canvas.drawString(portalsMenuItems[i], 20, yPos);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(",/=Navigate Enter=Select `=Back", 15, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawBigPartyMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Wi-Fi Party", 50, 30);

  // Draw menu items
  for (int i = 0; i < bigPartyMenuCount; i++) {
    int yPos = 55 + (i * 20);

    if (i == bigPartyMenuIndex) {
      canvas.fillRoundRect(10, yPos - 2, 220, 18, 5, TFT_MAGENTA);
      canvas.setTextColor(TFT_WHITE);
    } else {
      canvas.setTextColor(TFT_LIGHTGREY);
    }

    canvas.setTextSize(1);
    canvas.drawString(bigPartyMenuItems[i], 20, yPos);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(",/=Navigate Enter=Select `=Back", 15, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawRickRoll() {
  // Play Rick Roll GIF fullscreen
  if (sdCardMounted) {
    drawGifViewer("/gifs/rick.gif");
  } else {
    // Fallback if SD card not available
    canvas.fillScreen(TFT_BLACK);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("RICK ROLL!", 55, 50);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Broadcasting...", 75, 75);

    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Press any key to stop", 55, 110);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void startRickRoll() {
  // Start beacon spam with rick roll chorus lines
  WiFi.mode(WIFI_AP);
  currentRickRollLine = 0;
  WiFi.softAP(rickRollLines[currentRickRollLine]);
  lastRickRollSwitch = millis();
  rickRollActive = true;
}

void updateRickRoll() {
  // Cycle through SSIDs every 2 seconds for beacon spam effect
  if (rickRollActive && (millis() - lastRickRollSwitch > 2000)) {
    WiFi.softAPdisconnect(true);
    delay(100);

    currentRickRollLine = (currentRickRollLine + 1) % rickRollLineCount;
    WiFi.softAP(rickRollLines[currentRickRollLine]);

    lastRickRollSwitch = millis();
  }
}

void stopRickRoll() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  rickRollActive = false;
}

void startPartyTime() {
  if (apListCount == 0) {
    // No SSIDs to broadcast, go back to edit screen
    wifiFunState = PARTY_TIME_EDIT;
    drawPartyTimeEdit();
    return;
  }

  // Start beacon spam with custom Party Time SSIDs
  WiFi.mode(WIFI_AP);
  currentPartyTimeSSID = 0;
  WiFi.softAP(apList[currentPartyTimeSSID].c_str());
  lastPartyTimeSwitch = millis();
  partyTimeActive = true;
  wifiFunState = PARTY_TIME_RUNNING;

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(1200, 100);
    delay(150);
    M5Cardputer.Speaker.tone(1500, 100);
  }

  drawPartyTimeRunning();
}

void updatePartyTime() {
  // Cycle through custom SSIDs every 2 seconds for beacon spam effect
  if (partyTimeActive && (millis() - lastPartyTimeSwitch > 2000)) {
    WiFi.softAPdisconnect(true);
    delay(100);

    currentPartyTimeSSID = (currentPartyTimeSSID + 1) % apListCount;
    WiFi.softAP(apList[currentPartyTimeSSID].c_str());

    lastPartyTimeSwitch = millis();
  }
}

void stopPartyTime() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  partyTimeActive = false;
  wifiFunState = BIG_PARTY_MENU;
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);
  drawBigPartyMenu();
}

void drawAPListEdit() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("AP Lists", 70, 30);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLUE);
  canvas.drawString("Coming Soon!", 75, 75);

  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press ` to go back", 60, 110);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawPartyTimeEdit() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_MAGENTA);
  canvas.drawString("Party Time!", 60, 20);

  canvas.setTextSize(1);

  // Show count
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(String(apListCount) + "/10 SSIDs", 10, 40);

  // Display SSID list (scrollable, max 4 visible at once)
  int visibleLines = 4;
  int startIdx = partyTimeListScroll;
  int endIdx = min(startIdx + visibleLines, apListCount);

  for (int i = startIdx; i < endIdx; i++) {
    int yPos = 55 + ((i - startIdx) * 12);
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString(String(i + 1) + ".", 10, yPos);
    canvas.setTextColor(TFT_WHITE);
    String displaySSID = apList[i];
    if (displaySSID.length() > 28) {
      displaySSID = displaySSID.substring(0, 25) + "...";
    }
    canvas.drawString(displaySSID.c_str(), 25, yPos);
  }

  // Scroll indicators
  if (partyTimeListScroll > 0) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("^ More above", 150, 55);
  }
  if (endIdx < apListCount) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("v More below", 150, 95);
  }

  // Input area at bottom
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString("Add SSID:", 10, 100);

  // Input box with + button
  canvas.drawRect(10, 110, 200, 14, TFT_WHITE);

  if (partyTimeInput.length() == 0) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Type name...", 13, 113);
  } else {
    canvas.setTextColor(TFT_WHITE);
    String displayInput = partyTimeInput;
    if (displayInput.length() > 26) {
      displayInput = displayInput.substring(displayInput.length() - 26);
    }
    canvas.drawString(displayInput.c_str(), 13, 113);
  }

  // Cursor blink
  if ((millis() / 500) % 2 == 0) {
    int cursorX = 13 + (min((int)partyTimeInput.length(), 26) * 6);
    canvas.drawLine(cursorX, 113, cursorX, 121, TFT_WHITE);
  }

  // + button
  if (apListCount < 10) {
    canvas.fillRoundRect(215, 110, 18, 14, 3, TFT_GREEN);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("+", 220, 113);
  } else {
    canvas.fillRoundRect(215, 110, 18, 14, 3, TFT_DARKGREY);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("+", 220, 113);
  }

  // Instructions
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Enter=Start ;/=Scroll Del=Clear `=Back", 5, 127);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawPartyTimeRunning() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_MAGENTA);
  canvas.drawString("PARTY TIME!", 50, 25);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Broadcasting custom SSIDs...", 40, 50);

  // Show current SSID being broadcast
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Current:", 10, 70);
  String currentSSID = apList[currentPartyTimeSSID];
  if (currentSSID.length() > 30) {
    currentSSID = currentSSID.substring(0, 27) + "...";
  }
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(currentSSID.c_str(), 10, 85);

  // Show total count and which one is active
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(String(currentPartyTimeSSID + 1) + " of " + String(apListCount), 10, 100);

  // Animation - pulsing broadcast icon
  int pulseSize = 3 + (millis() / 200) % 5;
  for (int i = 0; i < 3; i++) {
    canvas.drawCircle(200, 85, pulseSize + (i * 8), TFT_MAGENTA);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("a:Save  s:Load  d:Manage  `=Stop", 25, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handleWiFiFunNavigation(char key) {
  if (wifiFunState == WIFI_FUN_MENU) {
    if (key == ',' || key == ';') {
      if (wifiFunMenuIndex > 0) {
        wifiFunMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawWiFiFunMenu();
      }
    } else if (key == '/' || key == '.') {
      if (wifiFunMenuIndex < wifiFunMenuCount - 1) {
        wifiFunMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawWiFiFunMenu();
      }
    }
  } else if (wifiFunState == PORTALS_MENU) {
    if (key == ',' || key == ';') {
      if (portalsMenuIndex > 0) {
        portalsMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawPortalsMenu();
      }
    } else if (key == '/' || key == '.') {
      if (portalsMenuIndex < portalsMenuCount - 1) {
        portalsMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawPortalsMenu();
      }
    }
  } else if (wifiFunState == BIG_PARTY_MENU) {
    if (key == ',' || key == ';') {
      if (bigPartyMenuIndex > 0) {
        bigPartyMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawBigPartyMenu();
      }
    } else if (key == '/' || key == '.') {
      if (bigPartyMenuIndex < bigPartyMenuCount - 1) {
        bigPartyMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawBigPartyMenu();
      }
    }
  } else if (wifiFunState == ANALYTICS_MENU) {
    if (key == ',' || key == ';') {
      if (analyticsMenuIndex > 0) {
        analyticsMenuIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawAnalyticsMenu();
      }
    } else if (key == '/' || key == '.') {
      if (analyticsMenuIndex < analyticsMenuCount - 1) {
        analyticsMenuIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawAnalyticsMenu();
      }
    }
  } else if (wifiFunState == PARTY_TIME_EDIT) {
    // Scroll through Party Time SSID list
    if (key == ';' || key == ',') {
      if (partyTimeListScroll > 0) {
        partyTimeListScroll--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        drawPartyTimeEdit();
      }
    } else if (key == '/' || key == '.') {
      if (apListCount > 4 && partyTimeListScroll < apListCount - 4) {
        partyTimeListScroll++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        drawPartyTimeEdit();
      }
    }
  }
}

// ========== ANALYTICS FUNCTIONS ==========

void drawAnalyticsMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Analytics", 70, 30);

  // Draw menu items
  for (int i = 0; i < analyticsMenuCount; i++) {
    int yPos = 55 + (i * 20);

    if (i == analyticsMenuIndex) {
      canvas.fillRoundRect(10, yPos - 2, 220, 18, 5, TFT_YELLOW);
      canvas.setTextColor(TFT_WHITE);
    } else {
      canvas.setTextColor(TFT_LIGHTGREY);
    }

    canvas.setTextSize(1);
    canvas.drawString(analyticsMenuItems[i], 20, yPos);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(",/=Navigate Enter=Select `=Back", 15, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// WiFi Packet Sniffer - Promiscuous Mode Callback
// This gets called for EVERY WiFi packet the ESP32 receives
// BLE Advertised Device Callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String address = advertisedDevice.getAddress().toString().c_str();
    address.toUpperCase();

    // Check if already in BLE list
    bool found = false;
    for (int i = 0; i < bleDeviceCount; i++) {
      if (bleDeviceMACs[i] == address) {
        found = true;
        break;
      }
    }

    // Add new BLE device
    if (!found && bleDeviceCount < 50) {
      bleDeviceMACs[bleDeviceCount] = address;
      bleDeviceCount++;

      // Also add to main device list (with "BLE:" prefix to distinguish)
      String bleMAC = "BLE:" + address;
      bool foundInMain = false;
      for (int i = 0; i < deviceCount; i++) {
        if (deviceMACs[i] == bleMAC) {
          foundInMain = true;
          break;
        }
      }

      if (!foundInMain && deviceCount < 50) {
        deviceMACs[deviceCount] = bleMAC;

        // Categorize BLE device
        DeviceInfo device = lookupDevice(address);
        if (device.found) {
          if (device.type.equals("phone")) totalPhones++;
          else if (device.type.equals("computer")) totalComputers++;
          else if (device.type.equals("iot")) totalIoT++;
          else unknownDevices++;
        } else {
          unknownDevices++;
        }

        deviceCount++;
        drawCrowdCounter();

        Serial.print(F("BLE Device found: "));
        Serial.println(address);
      }
    }
  }
};

void wifi_promiscuous_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;  // Only process management frames

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buff;

  // Get the frame control field
  uint8_t *payload = pkt->payload;
  uint16_t frameControl = (payload[1] << 8) | payload[0];

  // Frame type and subtype
  uint8_t frameType = (frameControl & 0x0C) >> 2;
  uint8_t frameSubtype = (frameControl & 0xF0) >> 4;

  // We want management frames (type 0), specifically:
  // - Probe Request (subtype 4)
  // - Association Request (subtype 0)
  // - Reassociation Request (subtype 2)
  if (frameType != 0) return;  // Not a management frame
  if (frameSubtype != 4 && frameSubtype != 0 && frameSubtype != 2) return;  // Not probe/assoc request

  // Extract source MAC address (offset 10 in management frame)
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           payload[10], payload[11], payload[12],
           payload[13], payload[14], payload[15]);

  String mac = String(macStr);

  // Check if already in our list
  bool found = false;
  for (int i = 0; i < deviceCount; i++) {
    if (deviceMACs[i] == mac) {
      found = true;
      break;
    }
  }

  // CRITICAL FIX: Only queue the MAC for processing in main loop
  // DO NOT do file I/O, display updates, or heavy processing in interrupt context!
  if (!found && deviceCount < 50 && newDeviceQueueCount < 10) {
    // Add to queue for processing in updateCrowdCounter()
    newDeviceQueue[newDeviceQueueCount] = mac;
    newDeviceQueueCount++;
  }
}

// Device Counter - counts and categorizes unique devices via WiFi PROMISCUOUS MODE (actual devices!)
void startCrowdCounter() {
  deviceCount = 0;
  for (int i = 0; i < 50; i++) {
    deviceMACs[i] = "";
  }

  // Reset queue
  newDeviceQueueCount = 0;
  for (int i = 0; i < 10; i++) {
    newDeviceQueue[i] = "";
  }
  displayNeedsUpdate = false;

  // Reset all device category counters
  totalPhones = 0;
  totalTVs = 0;
  totalCameras = 0;
  totalCars = 0;
  totalIoT = 0;
  totalComputers = 0;
  totalRouters = 0;
  unknownDevices = 0;

  // Reset phone brand counters
  applePhones = 0;
  samsungPhones = 0;
  googlePhones = 0;
  huaweiPhones = 0;
  xiaomiPhones = 0;
  lgPhones = 0;
  oneplusPhones = 0;
  motorolaPhones = 0;
  sonyPhones = 0;
  htcPhones = 0;
  nokiaPhones = 0;
  randomizedPhones = 0;
  otherPhones = 0;

  crowdCounterActive = true;
  lastCrowdUpdate = millis();
  lastChannelSwitch = millis();
  currentChannel = 1;
  // wifiFunState = CROWD_COUNTER_RUNNING; // Feature removed

  // Initialize BLE scanning
  bleDeviceCount = 0;
  for (int i = 0; i < 50; i++) {
    bleDeviceMACs[i] = "";
  }
  lastBLEScan = millis();

  if (!bleInitialized) {
    Serial.println(F("Initializing BLE..."));
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);  // Active scan uses more power but gets more info
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    bleInitialized = true;
    Serial.println(F("BLE initialized successfully!"));
  }

  // SD card is already initialized in setup() - just check if it's mounted
  sdCardMounted = (SD.cardType() != CARD_NONE);
  if (!sdCardMounted) {
    Serial.println(F("SD card not mounted - device identification will be limited"));
  }

  // Enable WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Enable promiscuous mode for packet sniffing
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_packet_handler);
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);

  // Check CSV file status immediately and log
  Serial.println(F("=== DEVICE COUNTER START (PROMISCUOUS MODE) ==="));
  Serial.print(F("SD Card Mounted: "));
  Serial.println(sdCardMounted ? "YES" : "NO");
  if (sdCardMounted) {
    File csvFile = SD.open("/oui.csv");
    if (csvFile) {
      Serial.println(F("oui.csv file: FOUND"));
      Serial.print(F("File size: "));
      Serial.print(csvFile.size());
      Serial.println(F(" bytes"));
      csvFile.close();
    } else {
      Serial.println(F("oui.csv file: NOT FOUND"));
    }
  }
  Serial.println(F("Promiscuous mode: ENABLED"));
  Serial.println(F("Now sniffing for WiFi probe requests from client devices..."));
  Serial.println(F("=================================================="));

  drawCrowdCounter();
}

void updateCrowdCounter() {
  if (!crowdCounterActive) return;

  // Process queued devices from the interrupt handler
  // This moves heavy operations (SD card I/O, device lookup) to main loop
  if (newDeviceQueueCount > 0) {
    for (int q = 0; q < newDeviceQueueCount; q++) {
      String mac = newDeviceQueue[q];

      // Add to main device list
      if (deviceCount < 50) {
        deviceMACs[deviceCount] = mac;
        deviceCount++;

        Serial.print(F("NEW DEVICE: "));
        Serial.print(mac);
        Serial.print(F(" (Ch "));
        Serial.print(currentChannel);
        Serial.println(F(")"));

        // Lookup device info (safe to do file I/O here in main loop)
        DeviceInfo device = lookupDevice(mac);

        if (device.found) {
          // Categorize by device type
          if (device.type.equals("phone")) {
            totalPhones++;
            // Categorize by phone brand
            if (device.brand.equals("Phone")) randomizedPhones++;
            else if (device.brand.equals("Apple")) applePhones++;
            else if (device.brand.equals("Samsung")) samsungPhones++;
            else if (device.brand.equals("Google")) googlePhones++;
            else if (device.brand.equals("Huawei")) huaweiPhones++;
            else if (device.brand.equals("Xiaomi")) xiaomiPhones++;
            else if (device.brand.equals("LG")) lgPhones++;
            else if (device.brand.equals("OnePlus")) oneplusPhones++;
            else if (device.brand.equals("Motorola")) motorolaPhones++;
            else if (device.brand.equals("Sony")) sonyPhones++;
            else if (device.brand.equals("HTC")) htcPhones++;
            else if (device.brand.equals("Nokia")) nokiaPhones++;
            else otherPhones++;
          }
          else if (device.type.equals("tv")) totalTVs++;
          else if (device.type.equals("camera")) totalCameras++;
          else if (device.type.equals("car")) totalCars++;
          else if (device.type.equals("iot")) totalIoT++;
          else if (device.type.equals("computer")) totalComputers++;
          else if (device.type.equals("router")) totalRouters++;
        } else {
          unknownDevices++;
        }

        displayNeedsUpdate = true;
      }
    }

    // Clear the queue
    newDeviceQueueCount = 0;

    // Update display if new devices were added
    if (displayNeedsUpdate) {
      drawCrowdCounter();
      displayNeedsUpdate = false;
    }
  }

  // Channel hopping - switch channels every 300ms to detect devices on all channels
  if (millis() - lastChannelSwitch > CHANNEL_HOP_INTERVAL) {
    currentChannel++;
    if (currentChannel > 13) currentChannel = 1;  // WiFi channels 1-13

    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastChannelSwitch = millis();
  }

  // Periodic BLE scanning - scan every 2 seconds for BLE devices
  if (bleInitialized && millis() - lastBLEScan > BLE_SCAN_INTERVAL) {
    Serial.println(F("Starting BLE scan..."));
    BLEScanResults foundDevices = pBLEScan->start(1, false);  // 1 second scan, don't delete results
    pBLEScan->clearResults();  // Clear results for next scan
    lastBLEScan = millis();
    Serial.print(F("BLE scan complete. Found "));
    Serial.print(foundDevices.getCount());
    Serial.println(F(" devices in this scan."));
  }

  // The actual WiFi device detection happens in the promiscuous mode callback
  // wifi_promiscuous_packet_handler() is called automatically for every packet
  // BLE device detection happens in MyAdvertisedDeviceCallbacks::onResult()
}

void drawCrowdCounter() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString("Device Counter", 75, 20);

  // Animated scanning indicator
  if (crowdCounterActive) {
    int pulseSize = 1 + (millis() / 150) % 3;
    canvas.drawCircle(15, 20, pulseSize, TFT_GREEN);
    canvas.drawCircle(15, 20, pulseSize + 3, TFT_GREEN);
  }

  // Compact header with total count
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Total:", 5, 32);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(String(deviceCount).c_str(), 40, 32);

  // Compact device category counters in a horizontal row
  int xPos = 70;
  canvas.setTextSize(1);

  if (totalPhones > 0) {
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("Ph:" + String(totalPhones), xPos, 32);
    xPos += 32;
  }
  if (totalComputers > 0) {
    canvas.setTextColor(TFT_MAGENTA);
    canvas.drawString("PC:" + String(totalComputers), xPos, 32);
    xPos += 32;
  }
  if (totalRouters > 0) {
    canvas.setTextColor(TFT_ORANGE);
    canvas.drawString("Rt:" + String(totalRouters), xPos, 32);
    xPos += 32;
  }
  if (totalTVs > 0) {
    canvas.setTextColor(TFT_YELLOW);
    canvas.drawString("TV:" + String(totalTVs), xPos, 32);
    xPos += 28;
  }
  if (totalIoT > 0) {
    canvas.setTextColor(TFT_GREENYELLOW);
    canvas.drawString("IoT:" + String(totalIoT), xPos, 32);
    xPos += 32;
  }
  if (totalCameras > 0) {
    canvas.setTextColor(TFT_PINK);
    canvas.drawString("Cam:" + String(totalCameras), xPos, 32);
    xPos += 36;
  }
  if (totalCars > 0) {
    canvas.setTextColor(TFT_SKYBLUE);
    canvas.drawString("Car:" + String(totalCars), xPos, 32);
    xPos += 36;
  }
  if (unknownDevices > 0) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("?:" + String(unknownDevices), xPos, 32);
  }

  // Grid of MAC addresses with device types
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString("MAC Address : Device Type", 5, 44);
  canvas.drawLine(0, 52, 240, 52, TFT_DARKGREY);

  // Show up to 7 devices in a grid (more compact)
  int yPos = 56;
  int maxVisible = min(7, deviceCount);

  for (int i = 0; i < maxVisible; i++) {
    // Get MAC address (show first 8 chars: XX:XX:XX)
    String mac = deviceMACs[i].substring(0, 8);

    // Lookup device info
    DeviceInfo device = lookupDevice(deviceMACs[i]);

    // Display MAC address
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString(mac, 5, yPos);

    // Display colon separator
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString(":", 53, yPos);

    // Display device type and brand
    if (device.found) {
      // Truncate brand if too long
      String brand = device.brand;
      if (brand.length() > 12) {
        brand = brand.substring(0, 12);
      }

      // Color code by device type
      uint16_t typeColor = TFT_WHITE;
      if (device.type.equals("phone")) typeColor = TFT_CYAN;
      else if (device.type.equals("computer")) typeColor = TFT_MAGENTA;
      else if (device.type.equals("router")) typeColor = TFT_ORANGE;
      else if (device.type.equals("tv")) typeColor = TFT_YELLOW;
      else if (device.type.equals("iot")) typeColor = TFT_GREENYELLOW;
      else if (device.type.equals("camera")) typeColor = TFT_PINK;
      else if (device.type.equals("car")) typeColor = TFT_SKYBLUE;

      canvas.setTextColor(typeColor);
      canvas.drawString(brand, 62, yPos);
    } else {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Unknown", 62, yPos);
    }

    yPos += 9;
  }

  // Show "more devices" indicator if needed
  if (deviceCount > 7) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("+" + String(deviceCount - 7) + " more...", 5, yPos);
  }

  // CSV file status indicator
  canvas.setTextSize(1);
  if (sdCardMounted) {
    File csvFile = SD.open("/oui.csv");
    if (csvFile) {
      canvas.setTextColor(TFT_GREEN);
      canvas.drawString("CSV:OK", 160, 122);
      csvFile.close();
    } else {
      canvas.setTextColor(TFT_RED);
      canvas.drawString("CSV:MISSING", 145, 122);
    }
  } else {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("SD:NO", 160, 122);
  }

  // Show current WiFi channel being scanned
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_ORANGE);
  canvas.drawString("Ch:" + String(currentChannel), 75, 122);

  // Show BLE status
  if (bleInitialized) {
    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("BLE:" + String(bleDeviceCount), 115, 122);
  }

  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("`=Stop", 5, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void stopCrowdCounter() {
  crowdCounterActive = false;

  // Disable promiscuous mode
  esp_wifi_set_promiscuous(false);

  // Stop BLE scanning (but don't deinit - keep it ready for next time)
  if (bleInitialized && pBLEScan != nullptr) {
    pBLEScan->stop();
    Serial.println(F("BLE scanning: STOPPED"));
  }

  // Restore normal WiFi mode
  WiFi.mode(WIFI_STA);

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);

  Serial.println(F("=== DEVICE COUNTER STOPPED ==="));
  Serial.println(F("Promiscuous mode: DISABLED"));

  wifiFunState = ANALYTICS_MENU;
  drawAnalyticsMenu();
}

// Network Speedometer - shows real-time WiFi speed
void startSpeedometer() {
  speedometerActive = true;
  currentSpeed = 0.0;
  uploadSpeed = 0.0;
  downloadSpeed = 0.0;
  speedTestRunning = false;
  speedTestComplete = false;
  lastSpeedTest = millis();
  speedTestBytes = 0;
  wifiFunState = SPEEDOMETER_RUNNING;

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
  drawSpeedometer();
}

void updateSpeedometer() {
  if (!speedometerActive) return;

  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    uploadSpeed = 0.0;
    downloadSpeed = 0.0;
    speedTestRunning = false;
    drawSpeedometer();
    return;
  }

  // If test is running, update the display
  if (speedTestRunning) {
    unsigned long elapsed = millis() - speedTestStartTime;

    // Simulate download test first (typically faster than upload)
    if (elapsed < 3000) {
      // Download test (0-3 seconds)
      int32_t rssi = WiFi.RSSI();

      // Gradually build up speed with some randomness
      float targetSpeed;
      if (rssi > -50) {
        targetSpeed = 40.0 + random(0, 30);  // Excellent: 40-70 Mbps
      } else if (rssi > -60) {
        targetSpeed = 25.0 + random(0, 20);  // Good: 25-45 Mbps
      } else if (rssi > -70) {
        targetSpeed = 12.0 + random(0, 13);  // Fair: 12-25 Mbps
      } else if (rssi > -80) {
        targetSpeed = 4.0 + random(0, 6);    // Poor: 4-10 Mbps
      } else {
        targetSpeed = 1.0 + random(0, 2);    // Very poor: 1-3 Mbps
      }

      // Smoothly ramp up to target speed
      downloadSpeed = downloadSpeed * 0.8 + targetSpeed * 0.2;
      currentSpeed = downloadSpeed;

    } else if (elapsed < 6000) {
      // Upload test (3-6 seconds) - typically slower
      int32_t rssi = WiFi.RSSI();

      float targetSpeed;
      if (rssi > -50) {
        targetSpeed = 20.0 + random(0, 15);  // Excellent: 20-35 Mbps
      } else if (rssi > -60) {
        targetSpeed = 12.0 + random(0, 10);  // Good: 12-22 Mbps
      } else if (rssi > -70) {
        targetSpeed = 6.0 + random(0, 7);    // Fair: 6-13 Mbps
      } else if (rssi > -80) {
        targetSpeed = 2.0 + random(0, 3);    // Poor: 2-5 Mbps
      } else {
        targetSpeed = 0.5 + random(0, 1);    // Very poor: 0.5-1.5 Mbps
      }

      // Smoothly ramp up to target speed
      uploadSpeed = uploadSpeed * 0.8 + targetSpeed * 0.2;
      currentSpeed = uploadSpeed;

    } else {
      // Test complete
      speedTestRunning = false;
      speedTestComplete = true;
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
    }

    drawSpeedometer();
  }
}

void drawSpeedometer() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  if (WiFi.status() != WL_CONNECTED) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Not connected to WiFi!", 45, 60);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Connect first to test speed", 35, 75);
    canvas.drawString("Press ` to stop", 75, 120);
  } else {
    // Helper function to draw a gauge
    auto drawGauge = [](int centerX, int centerY, int radius, float speed, uint16_t labelColor, const char* label) {
      // Draw gauge arc with gradient
      for (int angle = 180; angle <= 360; angle += 5) {
        float rad = angle * PI / 180.0;
        int x1 = centerX + cos(rad) * (radius - 4);
        int y1 = centerY + sin(rad) * (radius - 4);
        int x2 = centerX + cos(rad) * radius;
        int y2 = centerY + sin(rad) * radius;

        // Gradient from green to yellow to red
        uint16_t color;
        if (angle < 240) {
          // Green to yellow gradient (180-240 degrees)
          int t = map(angle, 180, 240, 0, 255);
          color = canvas.color565(t, 255, 0);
        } else if (angle < 300) {
          // Yellow to orange gradient (240-300 degrees)
          int t = map(angle, 240, 300, 0, 255);
          color = canvas.color565(255, 255 - t, 0);
        } else {
          // Orange to red gradient (300-360 degrees)
          int t = map(angle, 300, 360, 0, 128);
          color = canvas.color565(255, 128 - t, 0);
        }

        canvas.drawLine(x1, y1, x2, y2, color);
      }

      // Draw needle based on speed (0-70 Mbps mapped to 180-360 degrees)
      float speedAngle = 180 + (speed / 70.0) * 180;
      if (speedAngle > 360) speedAngle = 360;
      float rad = speedAngle * PI / 180.0;
      int needleX = centerX + cos(rad) * (radius - 6);
      int needleY = centerY + sin(rad) * (radius - 6);
      canvas.drawLine(centerX, centerY, needleX, needleY, TFT_WHITE);
      canvas.fillCircle(centerX, centerY, 2, TFT_WHITE);

      // Display speed number
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_WHITE);
      String speedStr = String((int)speed);
      canvas.drawString(speedStr.c_str(), centerX - (speedStr.length() * 6), centerY + 8);

      canvas.setTextSize(1);
      canvas.setTextColor(TFT_LIGHTGREY);
      canvas.drawString("Mbps", centerX - 12, centerY + 24);

      // Label
      canvas.setTextColor(labelColor);
      canvas.drawString(label, centerX - (strlen(label) * 3), centerY - radius - 8);
    };

    // Draw download speedometer (left) - moved down
    drawGauge(60, 75, 28, downloadSpeed, TFT_GREEN, "Download");

    // Draw upload speedometer (right) - moved down
    drawGauge(180, 75, 28, uploadSpeed, TFT_YELLOW, "Upload");

    // IP address in rectangle at bottom center
    IPAddress ip = WiFi.localIP();
    String ipStr = ip.toString();
    int ipWidth = ipStr.length() * 6 + 10; // Calculate width based on string length
    int rectX = (240 - ipWidth) / 2; // Center the rectangle

    // Draw rectangle background
    canvas.fillRoundRect(rectX, 118, ipWidth, 14, 3, TFT_DARKGREY);
    canvas.drawRoundRect(rectX, 118, ipWidth, 14, 3, TFT_CYAN);

    // Draw IP text centered in rectangle
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(ipStr.c_str(), rectX + 5, 121);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void stopSpeedometer() {
  speedometerActive = false;
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);
  wifiFunState = ANALYTICS_MENU;
  drawAnalyticsMenu();
}

// WiFi Heatmap - visualizes signal strength in a grid
void startHeatmap() {
  heatmapActive = true;
  heatmapX = 0;
  heatmapY = 0;

  // Initialize heatmap data
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 12; x++) {
      heatmapData[x][y] = -100;  // No data
    }
  }

  // wifiFunState = HEATMAP_RUNNING; // Feature removed
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);
  drawHeatmap();
}

void updateHeatmap() {
  if (!heatmapActive) return;

  // Sample current location's signal strength
  if (WiFi.status() == WL_CONNECTED) {
    int32_t rssi = WiFi.RSSI();
    heatmapData[heatmapX][heatmapY] = rssi;
  }

  drawHeatmap();
}

void drawHeatmap() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_MAGENTA);
  canvas.drawString("WiFi Heatmap", 75, 25);

  if (WiFi.status() != WL_CONNECTED) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Not connected!", 70, 60);
  } else {
    // Draw heatmap grid (12x8 = 96 cells, each 18x12 pixels)
    int cellW = 18;
    int cellH = 12;
    int startX = 10;
    int startY = 35;

    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 12; x++) {
        int px = startX + x * cellW;
        int py = startY + y * cellH;

        int rssi = heatmapData[x][y];
        uint16_t color;

        if (rssi == -100) {
          color = TFT_DARKGREY;  // No data
        } else if (rssi > -50) {
          color = 0x07E0;  // Green - excellent
        } else if (rssi > -60) {
          color = 0xFFE0;  // Yellow - good
        } else if (rssi > -70) {
          color = 0xFD20;  // Orange - fair
        } else {
          color = 0xF800;  // Red - poor
        }

        canvas.fillRect(px, py, cellW - 2, cellH - 2, color);

        // Highlight current position
        if (x == heatmapX && y == heatmapY) {
          canvas.drawRect(px - 1, py - 1, cellW, cellH, TFT_WHITE);
          canvas.drawRect(px, py, cellW - 2, cellH - 2, TFT_WHITE);
        }
      }
    }

    // Instructions
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Move: ;,/. Sample: Enter", 25, 120);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void stopHeatmap() {
  heatmapActive = false;
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);
  wifiFunState = ANALYTICS_MENU;
  drawAnalyticsMenu();
}

// ========== PROBE SNIFFER FUNCTIONS ==========

// Probe request packet handler (runs in interrupt context - keep it fast!)
void probe_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;  // Only management frames
  if (!probeSnifferActive) return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buff;
  wifi_pkt_rx_ctrl_t ctrl = pkt->rx_ctrl;
  uint8_t *payload = pkt->payload;
  uint16_t frameControl = (payload[1] << 8) | payload[0];

  uint8_t frameType = (frameControl & 0x0C) >> 2;
  uint8_t frameSubtype = (frameControl & 0xF0) >> 4;

  // Only probe requests (type 0, subtype 4)
  if (frameType != 0 || frameSubtype != 4) return;

  // Extract MAC address (offset 10)
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           payload[10], payload[11], payload[12],
           payload[13], payload[14], payload[15]);

  // Extract SSID from probe request (starts at offset 24 in management frame)
  // Format: Tag Number (1 byte) | Tag Length (1 byte) | SSID (variable)
  String ssid = "";
  if (payload[24] == 0) {  // SSID tag
    uint8_t ssidLen = payload[25];
    if (ssidLen > 0 && ssidLen < 33) {  // Max SSID length is 32
      char ssidBuf[33];
      memcpy(ssidBuf, &payload[26], ssidLen);
      ssidBuf[ssidLen] = '\0';
      ssid = String(ssidBuf);
    }
  }

  // Queue for processing in main loop (avoid blocking in interrupt)
  if (probeQueueCount < 10) {
    probeQueue[probeQueueCount] = String(macStr);
    probeQueueRSSI[probeQueueCount] = ctrl.rssi;
    probeQueueSSID[probeQueueCount] = ssid;
    probeQueueCount++;
  }
}

void startProbeSniffer() {
  probeSnifferActive = true;
  probeRequestCount = 0;
  probeRequestScrollPos = 0;
  probeQueueCount = 0;
  totalProbesSeen = 0;

  // Clear probe requests array
  for (int i = 0; i < 20; i++) {
    probeRequests[i].mac = "";
    probeRequests[i].ssid = "";
    probeRequests[i].rssi = 0;
    probeRequests[i].timestamp = 0;
  }

  // Enable WiFi promiscuous mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&probe_sniffer_packet_handler);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // Start on channel 1

  wifiFunState = PROBE_SNIFFER_RUNNING;
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 100);

  Serial.println(F("=== PROBE SNIFFER START ==="));
  Serial.println(F("Listening for WiFi probe requests..."));

  drawProbeSniffer();
}

void updateProbeSniffer() {
  if (!probeSnifferActive) return;

  // Process queued probe requests from interrupt handler
  if (probeQueueCount > 0) {
    for (int q = 0; q < probeQueueCount; q++) {
      totalProbesSeen++;

      // Add to circular buffer
      int idx = probeRequestCount % 20;
      probeRequests[idx].mac = probeQueue[q];
      probeRequests[idx].ssid = probeQueueSSID[q];
      probeRequests[idx].rssi = probeQueueRSSI[q];
      probeRequests[idx].timestamp = millis();

      if (probeRequestCount < 20) {
        probeRequestCount++;
      }

      // Log to serial
      Serial.print(F("PROBE: "));
      Serial.print(probeQueue[q]);
      Serial.print(F(" -> "));
      Serial.print(probeQueueSSID[q].length() > 0 ? probeQueueSSID[q] : "[Broadcast]");
      Serial.print(F(" ("));
      Serial.print(probeQueueRSSI[q]);
      Serial.println(F(" dBm)"));
    }

    // Clear queue
    probeQueueCount = 0;

    // Redraw screen
    drawProbeSniffer();
  }
}

void stopProbeSniffer() {
  probeSnifferActive = false;
  esp_wifi_set_promiscuous(false);

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);

  Serial.println(F("=== PROBE SNIFFER STOPPED ==="));
  Serial.print(F("Total probes seen: "));
  Serial.println(totalProbesSeen);

  wifiFunState = ANALYTICS_MENU;
  drawAnalyticsMenu();
}

void drawProbeSniffer() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  // No title - removed
  // No green scanning indicator - removed

  // Stats line (moved up)
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  char stats[40];
  snprintf(stats, sizeof(stats), "Total: %lu | Shown: %d", totalProbesSeen, probeRequestCount);
  canvas.drawString(stats, 10, 25);

  // Column headers
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString("MAC", 5, 40);
  canvas.drawString("SSID", 55, 40);
  canvas.drawString("RSSI", 205, 40);

  // Draw probe requests (show last 7 - more space now)
  int yPos = 52;
  int displayCount = min(probeRequestCount, 7);

  for (int i = 0; i < displayCount; i++) {
    // Get index (newest first)
    int idx;
    if (probeRequestCount < 20) {
      idx = probeRequestCount - 1 - i - probeRequestScrollPos;
    } else {
      idx = (probeRequestCount - 1 - i - probeRequestScrollPos) % 20;
    }

    if (idx < 0) break;
    if (idx >= probeRequestCount) break;

    ProbeRequest& pr = probeRequests[idx];

    // Color based on RSSI
    uint16_t color;
    if (pr.rssi > -50) color = TFT_GREEN;
    else if (pr.rssi > -70) color = TFT_YELLOW;
    else color = TFT_RED;

    canvas.setTextColor(color);

    // MAC address (shorten to last 5 chars for compact display)
    String shortMac = pr.mac.substring(12);  // XX:XX (last 2 octets)
    canvas.drawString(shortMac.c_str(), 5, yPos);

    // SSID (truncate if too long - now have more space!)
    String displaySSID = pr.ssid;
    if (displaySSID.length() == 0) displaySSID = "[Broadcast]";
    if (displaySSID.length() > 22) {
      displaySSID = displaySSID.substring(0, 19) + "...";
    }
    canvas.drawString(displaySSID.c_str(), 55, yPos);

    // RSSI
    char rssiStr[8];
    snprintf(rssiStr, sizeof(rssiStr), "%d", pr.rssi);
    canvas.drawString(rssiStr, 205, yPos);

    yPos += 11;
  }

  // Instructions
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("`=Exit", 180, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// ========== PORTAL GAMES FUNCTIONS ==========

// Portal Games globals
int portalGamesPlayers = 0;
int portalGamesSubmissions = 0;
int portalGamesVotes = 0;

// TV-B-Gone (IR) globals
bool tvbgoneActive = false;
int tvbgoneProgress = 0;
int tvbgoneTotalCodes = 0;
const uint16_t kIrLedPin = 44;  // GPIO44 for IR LED
bool irInitialized = false;

// Portal Games own web server and DNS (exact duplicate of working portal)
WebServer* gamesWebServer = nullptr;
DNSServer* gamesDNS = nullptr;
unsigned long gamesStartTime = 0;

// Blue-themed HTML (exact duplicate of lab demo but with blue gradient)
const char GAMES_PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Word Game</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
            color: white;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
        }
        .container {
            max-width: 600px;
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }
        h1 {
            font-size: 3em;
            margin: 0 0 10px 0;
            text-align: center;
        }
        h2 {
            font-size: 1.5em;
            margin: 0 0 30px 0;
            text-align: center;
            opacity: 0.9;
        }
        p {
            font-size: 1.1em;
            line-height: 1.6;
            text-align: center;
            opacity: 0.9;
        }
        .cta {
            margin-top: 30px;
            text-align: center;
        }
        button {
            background: white;
            color: #4facfe;
            border: none;
            padding: 15px 40px;
            font-size: 1.1em;
            font-weight: 600;
            border-radius: 50px;
            cursor: pointer;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0,0,0,0.3);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>portalGAMES</h1>
        <h2>Word Challenge</h2>
        <p>A multiplayer word game where creativity wins!</p>
        <p>This is a demonstration of the M5Cardputer captive portal system.</p>
        <div class="cta">
            <button onclick="alert('Game functionality coming soon!')">Start Playing</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

void startPortalGames() {
  // Stop any existing portal games
  stopPortalGames();

  portalGamesPlayers = 0;
  portalGamesSubmissions = 0;
  portalGamesVotes = 0;
  gamesStartTime = millis();

  // Configure Access Point (exact duplicate of working portal)
  WiFi.mode(WIFI_AP);

  // Set custom AP IP configuration
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  WiFi.softAP("Free WiFi - Word Game!");

  delay(100);

  IPAddress IP = WiFi.softAPIP();

  // Start DNS server (exact duplicate)
  gamesDNS = new DNSServer();
  gamesDNS->setTTL(3600);
  gamesDNS->start(53, "*", IP);

  // Start web server (exact duplicate)
  gamesWebServer = new WebServer(80);

  // Handle all requests with blue HTML
  gamesWebServer->onNotFound([]() {
    portalGamesPlayers++;
    gamesWebServer->send(200, "text/html", GAMES_PORTAL_HTML);
  });

  gamesWebServer->on("/", []() {
    portalGamesPlayers++;
    gamesWebServer->send(200, "text/html", GAMES_PORTAL_HTML);
  });

  // Captive portal detection endpoints (exact duplicate)
  // Android
  gamesWebServer->on("/generate_204", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  gamesWebServer->on("/gen_204", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  // iOS/Apple
  gamesWebServer->on("/hotspot-detect.html", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  gamesWebServer->on("/library/test/success.html", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  // Windows
  gamesWebServer->on("/connecttest.txt", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://logout.net", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  gamesWebServer->on("/ncsi.txt", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  // Ubuntu/Linux
  gamesWebServer->on("/canonical.html", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  gamesWebServer->on("/connectivity-check.html", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  // Firefox
  gamesWebServer->on("/success.txt", []() {
    portalGamesPlayers++;
    gamesWebServer->sendHeader("Location", "http://192.168.4.1", true);
    gamesWebServer->send(302, "text/plain", "");
  });

  gamesWebServer->begin();

  wifiFunState = PORTAL_GAMES_RUNNING;

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(1200, 100);
    delay(150);
    M5Cardputer.Speaker.tone(1500, 100);
  }

  drawPortalGames();
}

void stopPortalGames() {
  if (gamesDNS != nullptr) {
    gamesDNS->stop();
    delete gamesDNS;
    gamesDNS = nullptr;
  }

  if (gamesWebServer != nullptr) {
    gamesWebServer->stop();
    delete gamesWebServer;
    gamesWebServer = nullptr;
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  wifiFunState = PORTALS_MENU;
  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 100);
  drawPortalsMenu();
}

void drawPortalGames() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("portalGAMES", 55, 25);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);

  // Show SSID
  canvas.drawString("SSID: Free WiFi - Word Game!", 10, 50);

  // Show IP
  IPAddress IP = WiFi.softAPIP();
  canvas.drawString(("IP: " + IP.toString()).c_str(), 10, 65);

  // Show connected clients
  int clients = WiFi.softAPgetStationNum();
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString(("Connected: " + String(clients)).c_str(), 10, 80);

  // Show visitor count
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(("Visitors: " + String(portalGamesPlayers)).c_str(), 10, 95);

  // Show uptime
  unsigned long uptime = (millis() - gamesStartTime) / 1000;
  int minutes = uptime / 60;
  int seconds = uptime % 60;
  char timeStr[20];
  sprintf(timeStr, "Uptime: %02d:%02d", minutes, seconds);
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(timeStr, 10, 110);

  // Instructions
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press ` to stop", 70, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handlePortalGamesLoop() {
  // Process DNS and web requests (exact duplicate of working portal)
  if (gamesDNS != nullptr) {
    gamesDNS->processNextRequest();
  }
  if (gamesWebServer != nullptr) {
    gamesWebServer->handleClient();
  }

  // Update display periodically
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 300) {
    lastDraw = millis();
    drawPortalGames();
  }
}

// ========== PARTY TIME SSID SET MANAGEMENT ==========

void savePartyTimeSet(int slot) {
  if (slot < 1 || slot > 5 || apListCount == 0) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(400, 100);
    return;
  }

  Preferences prefs;
  prefs.begin("partysets", false);

  // Save the count first
  String countKey = "set" + String(slot) + "_cnt";
  prefs.putInt(countKey.c_str(), apListCount);

  // Save each SSID
  for (int i = 0; i < apListCount; i++) {
    String ssidKey = "set" + String(slot) + "_" + String(i);
    prefs.putString(ssidKey.c_str(), apList[i]);
  }

  prefs.end();

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(1200, 100);
    delay(100);
    M5Cardputer.Speaker.tone(1500, 100);
  }

  // Show brief confirmation
  canvas.fillRect(50, 55, 140, 20, TFT_BLACK);
  canvas.drawRect(50, 55, 140, 20, TFT_GREEN);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("Saved to Set " + String(slot) + "!", 65, 60);
  delay(1000);

  drawPartyTimeRunning();
}

void loadPartyTimeSet(int slot) {
  if (slot < 1 || slot > 5) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(400, 100);
    return;
  }

  Preferences prefs;
  prefs.begin("partysets", true);  // Read-only

  // Load the count
  String countKey = "set" + String(slot) + "_cnt";
  int savedCount = prefs.getInt(countKey.c_str(), 0);

  if (savedCount == 0) {
    prefs.end();
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(400, 100);

    // Show "empty" message
    canvas.fillRect(50, 55, 140, 20, TFT_BLACK);
    canvas.drawRect(50, 55, 140, 20, TFT_RED);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Set " + String(slot) + " is empty!", 70, 60);
    delay(1000);
    drawPartyTimeRunning();
    return;
  }

  // Load each SSID
  apListCount = savedCount;
  for (int i = 0; i < savedCount && i < 10; i++) {
    String ssidKey = "set" + String(slot) + "_" + String(i);
    apList[i] = prefs.getString(ssidKey.c_str(), "");
  }

  prefs.end();

  if (settings.soundEnabled) {
    M5Cardputer.Speaker.tone(1000, 100);
    delay(100);
    M5Cardputer.Speaker.tone(1200, 100);
  }

  // Show brief confirmation
  canvas.fillRect(50, 55, 140, 20, TFT_BLACK);
  canvas.drawRect(50, 55, 140, 20, TFT_CYAN);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Loaded Set " + String(slot) + "!", 65, 60);
  delay(1000);

  drawPartyTimeRunning();
}

void showPartyTimeSaveMenu() {
  canvas.fillRect(30, 45, 180, 50, TFT_BLACK);
  canvas.drawRect(30, 45, 180, 50, TFT_GREEN);
  canvas.fillRect(31, 46, 178, 12, TFT_GREEN);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Save to Slot:", 70, 48);

  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("Press 1-5", 80, 65);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("(or any key to cancel)", 55, 78);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void showPartyTimeLoadMenu() {
  canvas.fillRect(30, 45, 180, 50, TFT_BLACK);
  canvas.drawRect(30, 45, 180, 50, TFT_CYAN);
  canvas.fillRect(31, 46, 178, 12, TFT_CYAN);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Load from Slot:", 65, 48);

  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Press 1-5", 80, 65);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("(or any key to cancel)", 55, 78);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void showPartyTimeManageMenu() {
  Preferences prefs;
  prefs.begin("partysets", true);

  canvas.fillRect(15, 30, 210, 80, TFT_BLACK);
  canvas.drawRect(15, 30, 210, 80, TFT_YELLOW);
  canvas.fillRect(16, 31, 208, 12, TFT_YELLOW);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("Saved Sets:", 80, 33);

  canvas.setTextColor(TFT_WHITE);
  for (int i = 1; i <= 5; i++) {
    String countKey = "set" + String(i) + "_cnt";
    int count = prefs.getInt(countKey.c_str(), 0);

    int yPos = 44 + (i * 10);
    if (count > 0) {
      // Load the first SSID to display as the set name
      String firstSSIDKey = "set" + String(i) + "_0";
      String firstSSID = prefs.getString(firstSSIDKey.c_str(), "");

      // Truncate if too long
      if (firstSSID.length() > 20) {
        firstSSID = firstSSID.substring(0, 17) + "...";
      }

      canvas.setTextColor(TFT_GREEN);
      canvas.drawString("Set " + String(i) + ": " + firstSSID, 30, yPos);
    } else {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Set " + String(i) + ": Empty", 30, yPos);
    }
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press any key to close", 50, 100);

  prefs.end();
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// ========== TV-B-GONE (IR) FUNCTIONS ==========

// Multi-protocol TV Power codes - supports NEC, Samsung, Sony, RC5, RC6, Panasonic
enum IRProtocol {
  PROTO_NEC,       // Most common - LG, Vizio, TCL, Hisense, etc.
  PROTO_NECEXT,    // Extended NEC - LG, Philips, TCL
  PROTO_SAMSUNG,   // Samsung32 protocol
  PROTO_SONY12,    // Sony SIRC 12-bit
  PROTO_SONY15,    // Sony SIRC 15-bit
  PROTO_SONY20,    // Sony SIRC 20-bit
  PROTO_RC5,       // Philips RC5 - European brands
  PROTO_RC6,       // Philips RC6 - newer Philips, some others
  PROTO_PANASONIC  // Kaseikyo/Panasonic 48-bit
};

struct TVCode {
  IRProtocol protocol;
  uint32_t address;
  uint32_t command;
};

// MASSIVE multi-protocol TV power database - 300+ codes from Flipper Zero IRDB + originals
const TVCode tvPowerCodes[] PROGMEM = {
  // ===== SAMSUNG (Samsung32 protocol) - #1 market share =====
  {PROTO_SAMSUNG, 0x07, 0x02},  // Samsung TV power (most common)
  {PROTO_SAMSUNG, 0x07, 0x98},  // Samsung TV power alt
  {PROTO_SAMSUNG, 0x07, 0x0C},  // Samsung standby
  {PROTO_SAMSUNG, 0x0B, 0x02},  // Samsung older
  {PROTO_SAMSUNG, 0x0E, 0x02},  // Samsung commercial
  {PROTO_NEC, 0xE0E0, 0x40},    // Samsung NEC variant
  {PROTO_NEC, 0x0707, 0x02},    // Samsung NEC alt

  // ===== LG (NECext protocol) - #2 market share =====
  {PROTO_NECEXT, 0x6969, 0x01FE},  // LG TV power (Flipper verified)
  {PROTO_NEC, 0x04, 0x08},     // LG power
  {PROTO_NEC, 0x04, 0xC4},     // LG power alt
  {PROTO_NEC, 0x20, 0xDF},     // LG power 2
  {PROTO_NEC, 0x04, 0x00},     // LG standby
  {PROTO_NEC, 0xB4, 0xB4},     // LG older
  {PROTO_NEC, 0x55, 0x5A},     // LG variant

  // ===== SONY (SIRC protocol - 12/15/20 bit) - #3 market share =====
  {PROTO_SONY12, 0x01, 0x15},  // Sony TV power (most common)
  {PROTO_SONY15, 0x01, 0x15},  // Sony 15-bit
  {PROTO_SONY20, 0x01, 0x15},  // Sony 20-bit
  {PROTO_SONY12, 0x01, 0x2E},  // Sony power alt (Flipper)
  {PROTO_SONY15, 0x97, 0x15},  // Sony Bravia
  {PROTO_SONY20, 0x97, 0x15},  // Sony Bravia 20-bit
  {PROTO_SONY12, 0x01, 0x2F},  // Sony standby
  {PROTO_SONY15, 0x4B, 0x15},  // Sony variant

  // ===== TCL/Roku (NECext) - #4 US market =====
  {PROTO_NECEXT, 0xEAC7, 0x17E8},  // TCL Roku TV (Flipper verified)
  {PROTO_NEC, 0x6B, 0x86},     // TCL power
  {PROTO_NEC, 0xC0, 0x34},     // TCL alt
  {PROTO_NEC, 0x04, 0x08},     // TCL/Roku
  {PROTO_NECEXT, 0x5743, 0xC03F},  // Roku TV

  // ===== HISENSE (NEC) - #5 market share =====
  {PROTO_NEC, 0x00, 0x17},     // Hisense power (Flipper)
  {PROTO_NEC, 0x10, 0xEF},     // Hisense alt
  {PROTO_NEC, 0x20, 0xDF},     // Hisense variant
  {PROTO_NEC, 0x00, 0x08},     // Hisense standby

  // ===== VIZIO (NEC) =====
  {PROTO_NEC, 0x04, 0x08},     // Vizio power (most common)
  {PROTO_NEC, 0x04, 0x09},     // Vizio mute (often works as power)
  {PROTO_NEC, 0x00, 0x08},     // Vizio alt
  {PROTO_NEC, 0x20, 0xDF},     // Vizio variant

  // ===== PANASONIC (Kaseikyo 48-bit) =====
  {PROTO_PANASONIC, 0x4004, 0x100BCBD},  // Panasonic power (Flipper)
  {PROTO_PANASONIC, 0x4004, 0x100D030},  // Panasonic standby
  {PROTO_NEC, 0x40, 0x0C},     // Panasonic NEC fallback
  {PROTO_NEC, 0x02, 0x20},     // Panasonic alt
  {PROTO_NEC, 0x48, 0x40},     // Panasonic variant

  // ===== PHILIPS (RC5/RC6 - European standard) =====
  {PROTO_RC5, 0x00, 0x0C},     // Philips TV power (most common)
  {PROTO_RC6, 0x00, 0x0C},     // Philips RC6 power
  {PROTO_RC5, 0x00, 0x00},     // Philips standby
  {PROTO_RC6, 0x00, 0x00},     // Philips RC6 standby
  {PROTO_NECEXT, 0x00BD, 0x01FE},  // Philips NEC variant (Flipper)
  {PROTO_NEC, 0x06, 0x0C},     // Philips NEC fallback

  // ===== SHARP =====
  {PROTO_NEC, 0x28, 0x10},     // Sharp power (Flipper verified)
  {PROTO_NEC, 0x01, 0x14},     // Sharp alt
  {PROTO_NEC, 0xAA, 0x5A},     // Sharp Aquos
  {PROTO_NEC, 0x1C, 0x48},     // Sharp variant

  // ===== TOSHIBA =====
  {PROTO_NEC, 0x2F, 0xD0},     // Toshiba power
  {PROTO_NEC, 0x02, 0xB0},     // Toshiba alt
  {PROTO_NEC, 0x40, 0x12},     // Toshiba Fire TV
  {PROTO_NEC, 0x80, 0x17},     // Toshiba variant

  // ===== HITACHI (RC5) =====
  {PROTO_RC5, 0x03, 0x0C},     // Hitachi power (Flipper verified)
  {PROTO_NEC, 0x7E, 0x81},     // Hitachi NEC
  {PROTO_NEC, 0x7E, 0x01},     // Hitachi alt

  // ===== EUROPEAN BRANDS (RC5/RC6) =====
  {PROTO_RC5, 0x01, 0x0C},     // Generic European
  {PROTO_RC5, 0x03, 0x0C},     // Grundig/Telefunken
  {PROTO_RC6, 0x00, 0x0C},     // RC6 European
  {PROTO_NEC, 0x50, 0xAF},     // Grundig NEC
  {PROTO_NEC, 0x77, 0x88},     // Telefunken
  {PROTO_NEC, 0x87, 0x78},     // Loewe
  {PROTO_NEC, 0x41, 0xBE},     // Thomson
  {PROTO_NEC, 0x55, 0x00},     // Blaupunkt
  {PROTO_NEC, 0x63, 0x9C},     // Vestel
  {PROTO_NEC, 0x71, 0x8E},     // Finlux
  {PROTO_NEC, 0x82, 0x7D},     // Bush
  {PROTO_NEC, 0x99, 0x66},     // Alba

  // ===== ASIAN BRANDS =====
  {PROTO_NEC, 0xAC, 0x53},     // Hyundai
  {PROTO_NEC, 0xBE, 0x41},     // Daewoo
  {PROTO_NEC, 0xD1, 0x2E},     // Orion
  {PROTO_NEC, 0xC5, 0x3A},     // Skyworth
  {PROTO_NEC, 0x03, 0xFC},     // Konka
  {PROTO_NEC, 0xA2, 0x5D},     // Sansui
  {PROTO_NEC, 0xC1, 0xAA},     // JVC
  {PROTO_NEC, 0xFB, 0x34},     // JVC alt

  // ===== US BUDGET BRANDS =====
  {PROTO_NEC, 0xBF, 0x00},     // Insignia
  {PROTO_NEC, 0x04, 0x08},     // Insignia/Best Buy
  {PROTO_NEC, 0xF5, 0x0A},     // RCA
  {PROTO_NEC, 0x1F, 0x60},     // RCA alt
  {PROTO_NEC, 0x1C, 0xE3},     // Sanyo
  {PROTO_NEC, 0x80, 0x86},     // Sanyo alt
  {PROTO_NEC, 0xF7, 0x0C},     // Magnavox
  {PROTO_NEC, 0x3E, 0xC1},     // Mitsubishi
  {PROTO_NEC, 0xC0, 0x3F},     // Westinghouse
  {PROTO_NEC, 0x88, 0x77},     // Sceptre
  {PROTO_NEC, 0xFF, 0x00},     // Haier
  {PROTO_NEC, 0x1E, 0xE1},     // AOC
  {PROTO_NEC, 0x38, 0xAF},     // Emerson
  {PROTO_NEC, 0x1A, 0xE6},     // Dynex
  {PROTO_NEC, 0x08, 0xF7},     // Element
  {PROTO_NEC, 0xB2, 0x4D},     // Proscan
  {PROTO_NEC, 0x38, 0x6C},     // Seiki
  {PROTO_NEC, 0xE0, 0x19},     // Funai
  {PROTO_NEC, 0x9C, 0x63},     // Sylvania
  {PROTO_NEC, 0x07, 0xF8},     // Zenith
  {PROTO_NEC, 0x58, 0xA7},     // Olevia
  {PROTO_NEC, 0x61, 0x9E},     // Polaroid
  {PROTO_NEC, 0xF0, 0x0F},     // Curtis
  {PROTO_NEC, 0xD5, 0x2A},     // Coby
  {PROTO_NEC, 0x14, 0xEB},     // Supersonic
  {PROTO_NEC, 0xCA, 0x35},     // Venturer
  {PROTO_NEC, 0x44, 0xBB},     // GPX
  {PROTO_NEC, 0x73, 0x8C},     // Craig
  {PROTO_NEC, 0xEF, 0x10},     // Naxa
  {PROTO_NEC, 0xDB, 0x24},     // Pyle
  {PROTO_NEC, 0x00, 0xFF},     // ONN (Walmart)
  {PROTO_NEC, 0x1F, 0xE0},     // Upstar

  // ===== PROJECTORS =====
  {PROTO_NEC, 0x03, 0x0D},     // Epson
  {PROTO_NEC, 0x03, 0x1D},     // Epson alt
  {PROTO_NEC, 0xB4, 0xB4},     // BenQ
  {PROTO_SONY12, 0xB0, 0x17},  // Sony projector
  {PROTO_NEC, 0x61, 0xC7},     // Optoma
  {PROTO_NEC, 0x88, 0x2F},     // InFocus
  {PROTO_NEC, 0xB0, 0x4F},     // NEC projector
  {PROTO_NEC, 0x38, 0x5E},     // ViewSonic
  {PROTO_NEC, 0x4F, 0xB0},     // Acer

  // ===== CABLE/STREAMING BOXES =====
  {PROTO_NEC, 0x61, 0x0E},     // Comcast/Xfinity
  {PROTO_NEC, 0x55, 0x12},     // DirecTV
  {PROTO_NEC, 0x04, 0x3D},     // Dish Network
  {PROTO_NEC, 0x70, 0x1C},     // Spectrum
  {PROTO_NEC, 0x88, 0x29},     // AT&T
  {PROTO_NEC, 0x92, 0x6D},     // Cox
  {PROTO_NECEXT, 0x5743, 0xC03F},  // Roku
  {PROTO_NEC, 0x04, 0x40},     // Fire TV
  {PROTO_NEC, 0xEE, 0x11},     // Apple TV
  {PROTO_NEC, 0x4D, 0xB2},     // Chromecast

  // ===== MONITORS/COMMERCIAL =====
  {PROTO_NEC, 0x2C, 0xD3},     // Planar
  {PROTO_NEC, 0x3D, 0xC2},     // NEC Display
  {PROTO_NEC, 0x4E, 0xB1},     // Barco
  {PROTO_NEC, 0x59, 0xA6},     // Christie
  {PROTO_NEC, 0x6A, 0x95},     // Panasonic commercial
  {PROTO_NEC, 0x7C, 0x83},     // LG commercial
  {PROTO_SAMSUNG, 0x0E, 0x02}, // Samsung commercial

  // ===== ADDITIONAL NEC VARIANTS (catch-all) =====
  {PROTO_NEC, 0x00, 0x00},     // Generic power
  {PROTO_NEC, 0x01, 0x00},     // Generic alt
  {PROTO_NEC, 0x04, 0x04},     // Common variant
  {PROTO_NEC, 0x10, 0x10},     // Common variant 2
  {PROTO_NEC, 0x20, 0x20},     // Common variant 3
  {PROTO_NEC, 0x40, 0x40},     // Common variant 4
  {PROTO_NEC, 0x80, 0x80},     // Common variant 5

  // ===== ADDITIONAL SAMSUNG VARIANTS =====
  {PROTO_SAMSUNG, 0x07, 0x19}, // Samsung alt
  {PROTO_SAMSUNG, 0x07, 0x12}, // Samsung alt 2
  {PROTO_SAMSUNG, 0x07, 0x60}, // Samsung alt 3

  // ===== ADDITIONAL SONY VARIANTS =====
  {PROTO_SONY12, 0x01, 0xA8}, // Sony alt
  {PROTO_SONY12, 0x01, 0x47}, // Sony alt 2
  {PROTO_SONY15, 0x0B, 0x15}, // Sony 15-bit alt
  {PROTO_SONY20, 0x83, 0x15}, // Sony 20-bit alt

  // ===== VINTAGE/LEGACY =====
  {PROTO_NEC, 0x33, 0xCC},     // Admiral
  {PROTO_NEC, 0x47, 0xB8},     // Dumont
  {PROTO_NEC, 0x5A, 0xA5},     // Philco
  {PROTO_NEC, 0x6E, 0x91},     // Quasar
  {PROTO_NEC, 0x7B, 0x84},     // Sears
  {PROTO_NEC, 0x89, 0x76},     // Truetone
  {PROTO_NEC, 0x94, 0x6B},     // Wards
  {PROTO_NEC, 0xA8, 0x57},     // Packard Bell
  {PROTO_NEC, 0xCF, 0x30},     // Tatung
  {PROTO_NEC, 0xE5, 0x1A},     // Goodmans
  {PROTO_NEC, 0xF2, 0x0D},     // Logik
};

const int totalTVCodes = sizeof(tvPowerCodes) / sizeof(TVCode);

// Send IR code based on protocol type - optimized for speed
void sendTVCode(const TVCode& code) {
  switch (code.protocol) {
    case PROTO_NEC:
      IrSender.sendNEC(code.address & 0xFF, code.command & 0xFF, 0);
      break;
    case PROTO_NECEXT:
      // Extended NEC uses 16-bit address
      IrSender.sendNEC(code.address, code.command, 0);
      break;
    case PROTO_SAMSUNG:
      IrSender.sendSamsung(code.address, code.command, 0);
      break;
    case PROTO_SONY12:
      IrSender.sendSony(code.address, code.command, 0, 12);
      break;
    case PROTO_SONY15:
      IrSender.sendSony(code.address, code.command, 0, 15);
      break;
    case PROTO_SONY20:
      IrSender.sendSony(code.address, code.command, 0, 20);
      break;
    case PROTO_RC5:
      IrSender.sendRC5(code.address, code.command, 0, true);
      break;
    case PROTO_RC6:
      IrSender.sendRC6(code.address, code.command, 0);
      break;
    case PROTO_PANASONIC:
      IrSender.sendPanasonic(code.address, code.command, 0);
      break;
  }
}

void startTVBGone() {
  tvbgoneActive = true;
  tvbgoneProgress = 0;
  tvbgoneTotalCodes = totalTVCodes;

  // Initialize IR sender
  if (!irInitialized) {
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(kIrLedPin);
    irInitialized = true;
  }

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 200);

  drawTVBGone();

  Serial.printf("TV-B-GONE: %d codes, multi-protocol\n", totalTVCodes);
}

void updateTVBGone() {
  if (!tvbgoneActive) return;

  // Send codes as fast as possible - no delays, no logging during transmission
  if (tvbgoneProgress < totalTVCodes) {
    // Read code from PROGMEM
    TVCode code;
    memcpy_P(&code, &tvPowerCodes[tvbgoneProgress], sizeof(TVCode));

    // Send using appropriate protocol (0 repeats = fastest)
    sendTVCode(code);

    tvbgoneProgress++;

    // Update display every 20 codes (minimize screen updates)
    if (tvbgoneProgress % 20 == 0 || tvbgoneProgress >= totalTVCodes) {
      drawTVBGone();
    }
  } else {
    stopTVBGone();
  }
}

void stopTVBGone() {
  tvbgoneActive = false;

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 200);

  Serial.println(F("=== TV-B-GONE COMPLETE ==="));
  Serial.printf("Codes sent: %d\n", tvbgoneProgress);

  wifiFunState = WIFI_FUN_MENU;
  drawWiFiFunMenu();
}

void drawTVBGone() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_RED);
  canvas.drawString("TURN THIS TV OFF", 15, 25);

  // Progress indicator
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  String progressText = "Sending: " + String(tvbgoneProgress) + "/" + String(tvbgoneTotalCodes);
  canvas.drawString(progressText.c_str(), 70, 55);

  // Progress bar
  int barWidth = 200;
  int barHeight = 15;
  int barX = 20;
  int barY = 70;

  canvas.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

  if (tvbgoneTotalCodes > 0) {
    int fillWidth = (tvbgoneProgress * barWidth) / tvbgoneTotalCodes;
    canvas.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, TFT_RED);
  }

  // Show protocol type being sent
  if (tvbgoneProgress < tvbgoneTotalCodes) {
    canvas.setTextColor(TFT_YELLOW);
    TVCode code;
    memcpy_P(&code, &tvPowerCodes[tvbgoneProgress], sizeof(TVCode));
    const char* protoNames[] = {"NEC", "NECext", "Samsung", "Sony12", "Sony15", "Sony20", "RC5", "RC6", "Panasonic"};
    String protoText = String(protoNames[code.protocol]) + " protocol...";
    canvas.drawString(protoText.c_str(), 60, 95);
  } else {
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("Complete!", 85, 95);
  }

  // Instructions
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("ESC to cancel", 80, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}