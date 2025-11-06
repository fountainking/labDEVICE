#include "security_manager.h"
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <WiFi.h>
#include <SD.h>

SecurityManager securityManager;

// ============================================================================
// DEVICE PIN SYSTEM (shared across all apps)
// ============================================================================

bool DevicePIN::isSet() {
  Preferences prefs;
  prefs.begin("device", true);
  bool exists = prefs.isKey("pin");
  prefs.end();
  return exists;
}

bool DevicePIN::verify(String pin) {
  if (pin.length() != PIN_LENGTH) return false;

  Preferences prefs;
  prefs.begin("device", true);
  String stored = prefs.getString("pin", "");
  prefs.end();

  return pin == stored;
}

bool DevicePIN::create(String pin) {
  if (pin.length() != PIN_LENGTH) return false;

  // Validate characters (alphanumeric + symbols)
  for (int i = 0; i < pin.length(); i++) {
    char c = pin.charAt(i);
    if (!((c >= '0' && c <= '9') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') ||
          c == '!' || c == '@' || c == '#' || c == '$' || c == '%' ||
          c == '^' || c == '&' || c == '*' || c == '(' || c == ')')) {
      return false;
    }
  }

  Preferences prefs;
  prefs.begin("device", false);
  prefs.putString("pin", pin);
  prefs.end();
  return true;
}

bool DevicePIN::change(String oldPin, String newPin) {
  if (!verify(oldPin)) return false;
  return create(newPin);
}

void DevicePIN::factoryReset() {
  // Check for reset token on SD card
  if (SD.exists("/reset_pin.txt")) {
    Preferences prefs;
    prefs.begin("device", false);
    prefs.remove("pin");
    prefs.end();
    SD.remove("/reset_pin.txt");
  }
}

// ============================================================================
// SECURITY MANAGER
// ============================================================================

SecurityManager::SecurityManager() : initialized(false) {
  memset(hmacKey, 0, HMAC_KEY_SIZE);
  memset(pmk, 0, PMK_SIZE);
  memset(deviceID, 0, DEVICE_ID_SIZE);
  memset(networkName, 0, NETWORK_NAME_SIZE);
}

void SecurityManager::generateDeviceID() {
  // Generate unique device ID from ESP32 MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Create readable hex ID
  snprintf(deviceID, DEVICE_ID_SIZE, "LAB%02X%02X%02X%02X",
           mac[2], mac[3], mac[4], mac[5]);
}

void SecurityManager::deriveKeysFromPassword(String password) {
  // Use SHA-256 to derive keys from password
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  // Derive HMAC key
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)"HMAC", 4);
  mbedtls_sha256_update(&ctx, (const unsigned char*)password.c_str(), password.length());
  mbedtls_sha256_finish(&ctx, hmacKey);

  // Derive PMK (first 16 bytes of different hash)
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)"PMK", 3);
  mbedtls_sha256_update(&ctx, (const unsigned char*)password.c_str(), password.length());
  uint8_t pmkHash[32];
  mbedtls_sha256_finish(&ctx, pmkHash);
  memcpy(pmk, pmkHash, PMK_SIZE);

  mbedtls_sha256_free(&ctx);
}

bool SecurityManager::createNetwork(String password, String name) {
  if (password.length() < 8) return false;
  if (name.length() == 0 || name.length() > NETWORK_NAME_SIZE - 1) return false;

  generateDeviceID();
  deriveKeysFromPassword(password);
  strncpy(networkName, name.c_str(), NETWORK_NAME_SIZE - 1);
  networkName[NETWORK_NAME_SIZE - 1] = '\0';

  initialized = true;
  saveToPreferences();
  return true;
}

bool SecurityManager::joinNetwork(String password) {
  if (password.length() < 8) return false;

  generateDeviceID();
  deriveKeysFromPassword(password);

  // Generate color-based room name from password hash
  const char* colors[] = {
    "Red", "Orange", "Yellow", "Green", "Blue",
    "Purple", "Pink", "Cyan", "Magenta", "Black",
    "White", "Gray", "Crimson", "Gold", "Silver",
    "Violet", "Indigo", "Lime", "Navy", "Teal"
  };

  mbedtls_sha256_context ctx;
  uint8_t hash[32];
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)password.c_str(), password.length());
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  // Use hash to pick a color (20 options)
  int colorIndex = hash[0] % 20;
  snprintf(networkName, NETWORK_NAME_SIZE, "%s Room", colors[colorIndex]);

  initialized = true;
  // Don't save to preferences - rooms are temporary until device powers off
  // saveToPreferences();
  return true;
}

void SecurityManager::leaveNetwork() {
  initialized = false;
  clearPreferences();
  memset(hmacKey, 0, HMAC_KEY_SIZE);
  memset(pmk, 0, PMK_SIZE);
  memset(networkName, 0, NETWORK_NAME_SIZE);
}

bool SecurityManager::isConnected() {
  return initialized;
}

void SecurityManager::generateHMAC(const uint8_t* data, size_t len, uint8_t* hmac) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, hmacKey, HMAC_KEY_SIZE);
  mbedtls_md_hmac_update(&ctx, data, len);
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);
}

bool SecurityManager::verifyHMAC(const uint8_t* data, size_t len, const uint8_t* hmac) {
  uint8_t calculated[32];
  generateHMAC(data, len, calculated);
  return memcmp(calculated, hmac, 32) == 0;
}

void SecurityManager::getRotatedMAC(uint8_t* mac, uint32_t timestamp) {
  // Generate predictable MAC from timestamp and HMAC key
  // Rotates every 300 seconds (5 minutes) for privacy
  uint32_t rotation = timestamp / 300;

  uint8_t seed[4];
  seed[0] = (rotation >> 24) & 0xFF;
  seed[1] = (rotation >> 16) & 0xFF;
  seed[2] = (rotation >> 8) & 0xFF;
  seed[3] = rotation & 0xFF;

  uint8_t hash[32];
  generateHMAC(seed, 4, hash);

  // Use first 6 bytes of hash as MAC, ensure locally administered bit
  memcpy(mac, hash, 6);
  mac[0] = (mac[0] & 0xFE) | 0x02; // Set locally administered bit, clear multicast
}

bool SecurityManager::isValidRotatedMAC(const uint8_t* mac, uint32_t timestamp) {
  // Check current rotation and previous two (allow 10 min clock skew)
  uint32_t rotation = timestamp / 300;

  for (int i = -2; i <= 0; i++) {
    uint8_t expected[6];
    getRotatedMAC(expected, (rotation + i) * 300);
    if (memcmp(mac, expected, 6) == 0) {
      return true;
    }
  }
  return false;
}

void SecurityManager::saveToPreferences() {
  Preferences prefs;
  prefs.begin("labchat", false);
  prefs.putBytes("hmac", hmacKey, HMAC_KEY_SIZE);
  prefs.putBytes("pmk", pmk, PMK_SIZE);
  prefs.putString("deviceID", deviceID);
  prefs.putString("network", networkName);
  prefs.putBool("init", initialized);
  prefs.end();
}

bool SecurityManager::loadFromPreferences() {
  Preferences prefs;
  prefs.begin("labchat", true);

  if (!prefs.isKey("init")) {
    prefs.end();
    return false;
  }

  initialized = prefs.getBool("init", false);
  if (!initialized) {
    prefs.end();
    return false;
  }

  prefs.getBytes("hmac", hmacKey, HMAC_KEY_SIZE);
  prefs.getBytes("pmk", pmk, PMK_SIZE);
  String devID = prefs.getString("deviceID", "");
  String netName = prefs.getString("network", "");
  prefs.end();

  strncpy(deviceID, devID.c_str(), DEVICE_ID_SIZE - 1);
  strncpy(networkName, netName.c_str(), NETWORK_NAME_SIZE - 1);

  return true;
}

void SecurityManager::clearPreferences() {
  Preferences prefs;
  prefs.begin("labchat", false);
  prefs.clear();
  prefs.end();
}
