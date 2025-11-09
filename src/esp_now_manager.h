#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ESP-NOW configuration
#define MAX_PEERS 10
#define ESP_NOW_CHANNEL 1

// Peer info structure
struct PeerDevice {
  uint8_t mac[6];
  char deviceID[16];
  char username[16];
  char roomName[32];  // Room/network name for discovery
  unsigned long lastSeen;
  bool active;
  int rssi;  // Signal strength (raw)
  int rssiSmoothed;  // Smoothed RSSI for display
};

// ESP-NOW Manager
class ESPNowManager {
private:
  bool initialized;
  uint8_t broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  PeerDevice peers[MAX_PEERS];
  int peerCount;

  static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
  static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);

public:
  ESPNowManager();

  // Initialization
  bool init(const uint8_t* pmk);
  void deinit();
  bool isInitialized() { return initialized; }

  // Peer management
  bool addPeer(const uint8_t* mac, const char* deviceID, const char* username, const char* roomName = "", bool announce = true);
  bool removePeer(const uint8_t* mac);
  void updatePeerActivity(const uint8_t* mac, int rssi = -100);
  PeerDevice* findPeer(const uint8_t* mac);
  PeerDevice* findPeerByDeviceID(const char* deviceID);
  int getPeerCount() { return peerCount; }
  PeerDevice* getPeer(int index);
  void cleanupInactivePeers(); // Remove peers not seen in 5 minutes

  // Sending
  bool sendBroadcast(const uint8_t* data, size_t len);
  bool sendDirect(const uint8_t* mac, const uint8_t* data, size_t len);
  bool sendToDeviceID(const char* deviceID, const uint8_t* data, size_t len);

  // Statistics
  unsigned long bytesSent;
  unsigned long bytesReceived;
  unsigned long messagesSent;
  unsigned long messagesReceived;
  unsigned long sendFailures;
};

extern ESPNowManager espNowManager;

// Callback for received messages (implemented in message_handler)
extern void onMessageReceived(const uint8_t* mac, const uint8_t* data, int len);

#endif
