#include "esp_now_manager.h"
#include "message_handler.h"
#include <esp_wifi.h>

ESPNowManager espNowManager;

ESPNowManager::ESPNowManager() : initialized(false), peerCount(0),
                                  bytesSent(0), bytesReceived(0),
                                  messagesSent(0), messagesReceived(0),
                                  sendFailures(0) {
  memset(peers, 0, sizeof(peers));
}

void ESPNowManager::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  Serial.printf("ESP-NOW RECEIVE CALLBACK! len=%d\n", len);
  Serial.printf("  From MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  espNowManager.bytesReceived += len;
  espNowManager.messagesReceived++;

  // Estimate RSSI based on successful reception
  // Fresh reception = strong signal (-30 to -40 dBm)
  // This will degrade based on time in updateRadarDeviceList()
  int estimatedRSSI = -30;

  // Update peer activity with estimated RSSI
  espNowManager.updatePeerActivity(mac, estimatedRSSI);

  Serial.println("  Forwarding to message handler...");
  // Forward to message handler
  onMessageReceived(mac, data, len);
  Serial.println("  Message handler returned");
}

void ESPNowManager::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    espNowManager.sendFailures++;
  }
}

bool ESPNowManager::init(const uint8_t* pmk) {
  if (initialized) return true;

  Serial.println("ESPNowManager::init() - Initializing ESP-NOW...");

  // Set WiFi mode to STA (ESP-NOW requirement)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Set WiFi channel explicitly
  esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("  WiFi channel set to %d\n", ESP_NOW_CHANNEL);

  // Initialize ESP-NOW
  Serial.println("  Calling esp_now_init()...");
  esp_err_t initResult = esp_now_init();
  if (initResult != ESP_OK) {
    Serial.printf("  ERROR: esp_now_init() failed with code %d\n", initResult);
    return false;
  }
  Serial.println("  ESP-NOW initialized successfully");

  // Set PMK for encryption
  Serial.println("  Setting PMK...");
  esp_err_t pmkResult = esp_now_set_pmk(pmk);
  if (pmkResult != ESP_OK) {
    Serial.printf("  ERROR: esp_now_set_pmk() failed with code %d\n", pmkResult);
    esp_now_deinit();
    return false;
  }
  Serial.println("  PMK set successfully");

  // Register callbacks
  Serial.println("  Registering callbacks...");
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  Serial.println("  Callbacks registered");

  // Add broadcast peer (unencrypted for discovery)
  Serial.println("  Adding broadcast peer...");
  Serial.printf("  Broadcast MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                broadcastMAC[0], broadcastMAC[1], broadcastMAC[2],
                broadcastMAC[3], broadcastMAC[4], broadcastMAC[5]);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMAC, 6);
  peerInfo.channel = 0;  // 0 = use current WiFi channel
  peerInfo.encrypt = false;

  esp_err_t addPeerResult = esp_now_add_peer(&peerInfo);
  if (addPeerResult != ESP_OK) {
    Serial.printf("  ERROR: esp_now_add_peer() BROADCAST failed with code %d\n", addPeerResult);
    esp_now_deinit();
    return false;
  }
  Serial.println("  Broadcast peer added successfully");

  initialized = true;
  return true;
}

void ESPNowManager::deinit() {
  if (!initialized) return;

  esp_now_deinit();
  initialized = false;
  peerCount = 0;
  memset(peers, 0, sizeof(peers));
}

bool ESPNowManager::addPeer(const uint8_t* mac, const char* deviceID, const char* username, const char* roomName, bool announce) {
  Serial.printf("ESPNowManager::addPeer() called - DeviceID: %s, Username: %s, Room: %s, Announce: %d\n", deviceID, username, roomName, announce);
  Serial.printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("  Current peer count: %d/%d\n", peerCount, MAX_PEERS);

  if (peerCount >= MAX_PEERS) {
    Serial.println("  ERROR: Peer limit reached!");
    return false;
  }

  // Check if peer already exists
  PeerDevice* existing = findPeer(mac);
  if (existing) {
    Serial.println("  Peer already exists, updating info");
    // Update existing peer
    strncpy(existing->deviceID, deviceID, 15);
    existing->deviceID[15] = '\0';
    strncpy(existing->username, username, 15);
    existing->username[15] = '\0';
    if (roomName && strlen(roomName) > 0) {
      strncpy(existing->roomName, roomName, 31);
      existing->roomName[31] = '\0';
    }
    existing->lastSeen = millis();
    existing->active = true;
    // rssi will be updated by updatePeerActivity when messages arrive
    return true;
  }

  // Add new peer to ESP-NOW
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;  // 0 = use current WiFi channel
  peerInfo.encrypt = true;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.printf("  ERROR: esp_now_add_peer() failed with code %d\n", result);
    return false;
  }

  Serial.println("  SUCCESS: Peer added to ESP-NOW");

  // Add to our peer list
  memcpy(peers[peerCount].mac, mac, 6);
  strncpy(peers[peerCount].deviceID, deviceID, 15);
  peers[peerCount].deviceID[15] = '\0';
  strncpy(peers[peerCount].username, username, 15);
  peers[peerCount].username[15] = '\0';
  if (roomName && strlen(roomName) > 0) {
    strncpy(peers[peerCount].roomName, roomName, 31);
    peers[peerCount].roomName[31] = '\0';
  } else {
    peers[peerCount].roomName[0] = '\0';
  }
  peers[peerCount].lastSeen = millis();
  peers[peerCount].active = true;
  peers[peerCount].rssi = -100;  // Default RSSI
  peers[peerCount].rssiSmoothed = -100;  // Initialize smoothed value
  peerCount++;

  // Announce user arrival (system message) - only if requested
  if (announce) {
    String joinMsg = String(username) + " joined";
    messageHandler.addSystemMessage(joinMsg.c_str());
  }

  return true;
}

bool ESPNowManager::removePeer(const uint8_t* mac) {
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peers[i].mac, mac, 6) == 0) {
      // Remove from ESP-NOW
      esp_now_del_peer(mac);

      // Remove from our list (shift remaining)
      for (int j = i; j < peerCount - 1; j++) {
        peers[j] = peers[j + 1];
      }
      peerCount--;
      memset(&peers[peerCount], 0, sizeof(PeerDevice));
      return true;
    }
  }
  return false;
}

void ESPNowManager::updatePeerActivity(const uint8_t* mac, int rssi) {
  PeerDevice* peer = findPeer(mac);
  if (peer) {
    peer->lastSeen = millis();
    peer->active = true;
    peer->rssi = rssi;  // Store raw signal strength

    // Exponential smoothing: smoothed = 0.7 * old + 0.3 * new
    // This reduces jitter while still responding to changes
    if (peer->rssiSmoothed == 0) {
      peer->rssiSmoothed = rssi;  // Initialize on first update
    } else {
      peer->rssiSmoothed = (peer->rssiSmoothed * 7 + rssi * 3) / 10;
    }
  }
}

PeerDevice* ESPNowManager::findPeer(const uint8_t* mac) {
  for (int i = 0; i < peerCount; i++) {
    if (memcmp(peers[i].mac, mac, 6) == 0) {
      return &peers[i];
    }
  }
  return nullptr;
}

PeerDevice* ESPNowManager::findPeerByDeviceID(const char* deviceID) {
  for (int i = 0; i < peerCount; i++) {
    if (strcmp(peers[i].deviceID, deviceID) == 0) {
      return &peers[i];
    }
  }
  return nullptr;
}

PeerDevice* ESPNowManager::getPeer(int index) {
  if (index < 0 || index >= peerCount) return nullptr;
  return &peers[index];
}

void ESPNowManager::cleanupInactivePeers() {
  unsigned long now = millis();
  for (int i = peerCount - 1; i >= 0; i--) {
    if (now - peers[i].lastSeen > 300000) { // 5 minutes
      removePeer(peers[i].mac);
    }
  }
}

bool ESPNowManager::sendBroadcast(const uint8_t* data, size_t len) {
  if (!initialized) {
    Serial.println("sendBroadcast: ERROR - Not initialized!");
    return false;
  }

  Serial.printf("sendBroadcast: Sending %d bytes to broadcast MAC\n", len);
  Serial.printf("  Broadcast MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                broadcastMAC[0], broadcastMAC[1], broadcastMAC[2],
                broadcastMAC[3], broadcastMAC[4], broadcastMAC[5]);

  esp_err_t result = esp_now_send(broadcastMAC, data, len);
  if (result == ESP_OK) {
    Serial.println("  SUCCESS: Broadcast sent");
    bytesSent += len;
    messagesSent++;
    return true;
  }
  Serial.printf("  ERROR: esp_now_send failed with code %d\n", result);
  sendFailures++;
  return false;
}

bool ESPNowManager::sendDirect(const uint8_t* mac, const uint8_t* data, size_t len) {
  if (!initialized) return false;

  esp_err_t result = esp_now_send(mac, data, len);
  if (result == ESP_OK) {
    bytesSent += len;
    messagesSent++;
    return true;
  }
  sendFailures++;
  return false;
}

bool ESPNowManager::sendToDeviceID(const char* deviceID, const uint8_t* data, size_t len) {
  PeerDevice* peer = findPeerByDeviceID(deviceID);
  if (!peer) return false;
  return sendDirect(peer->mac, data, len);
}
