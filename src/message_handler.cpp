#include "message_handler.h"
#include "security_manager.h"
#include "esp_now_manager.h"
#include "labchat.h"
#include <time.h>
#include <SD.h>

MessageHandler messageHandler;

MessageHandler::MessageHandler() : queueHead(0), queueTail(0), queueCount(0), onMessageAddedCallback(nullptr),
                                    emojiChunkCallback(nullptr),
                                    receiveBuffer(nullptr), receiveBufferSize(0), receivedBytes(0), isReceivingFile(false) {
  memset(messageQueue, 0, sizeof(messageQueue));
}

bool MessageHandler::createMessage(MessageType type, const char* content,
                                    uint8_t channel, const char* targetID,
                                    SecureMessage* outMsg) {
  if (!securityManager.isConnected()) return false;

  memset(outMsg, 0, sizeof(SecureMessage));

  // Fill header
  outMsg->type = type;
  outMsg->version = PROTOCOL_VERSION;
  outMsg->timestamp = time(nullptr);

  // Device info
  strncpy(outMsg->deviceID, securityManager.getDeviceID(), 15);
  outMsg->deviceID[15] = '\0';

  // Username (load from preferences or generate User# from device ID)
  Preferences prefs;
  prefs.begin("labchat", true);
  String defaultUsername = "User";
  const char* devID = securityManager.getDeviceID();
  // Extract last 4 chars of device ID to create unique number
  int userNum = 0;
  int len = strlen(devID);
  if (len >= 4) {
    for (int i = len - 4; i < len; i++) {
      userNum = userNum * 10 + (devID[i] % 10);
    }
  }
  defaultUsername += String(userNum % 10000); // Keep it 4 digits max
  String username = prefs.getString("username", defaultUsername);
  prefs.end();
  strncpy(outMsg->username, username.c_str(), 15);
  outMsg->username[15] = '\0';

  // Channel and target
  outMsg->channel = channel;
  if (targetID) {
    strncpy(outMsg->targetID, targetID, 15);
    outMsg->targetID[15] = '\0';
  }

  // Payload
  size_t contentLen = strlen(content);
  if (contentLen > sizeof(outMsg->payload) - 1) {
    contentLen = sizeof(outMsg->payload) - 1;
  }
  outMsg->msgLen = contentLen;
  strncpy(outMsg->payload, content, contentLen);
  outMsg->payload[contentLen] = '\0';

  // Generate HMAC (over everything except HMAC field itself)
  size_t msgSize = sizeof(SecureMessage) - sizeof(outMsg->hmac);
  securityManager.generateHMAC((uint8_t*)outMsg, msgSize, outMsg->hmac);

  return true;
}

bool MessageHandler::verifyMessage(const SecureMessage* msg) {
  // Check version
  if (msg->version != PROTOCOL_VERSION) {
    Serial.printf("  Version mismatch: got %d, expected %d\n", msg->version, PROTOCOL_VERSION);
    return false;
  }

  // Verify HMAC
  size_t msgSize = sizeof(SecureMessage) - sizeof(msg->hmac);
  if (!securityManager.verifyHMAC((uint8_t*)msg, msgSize, msg->hmac)) {
    Serial.println("  HMAC verification failed!");
    return false;
  }

  // Timestamp check disabled - offline mesh doesn't have clock sync
  uint32_t now = time(nullptr);
  int32_t timeDiff = (int32_t)(msg->timestamp - now);
  Serial.printf("  Timestamp check: msg=%u, now=%u, diff=%d (check disabled)\n", msg->timestamp, now, timeDiff);
  // Skip timestamp validation for offline operation

  return true;
}

bool MessageHandler::handleReceivedMessage(const uint8_t* mac, const uint8_t* data, int len) {
  Serial.printf("MessageHandler::handleReceivedMessage() - len=%d\n", len);

  // Check if it's a discovery beacon (unencrypted)
  if (len == sizeof(DiscoveryBeacon)) {
    const DiscoveryBeacon* beacon = (const DiscoveryBeacon*)data;
    if (beacon->type == MSG_BEACON && beacon->version == PROTOCOL_VERSION) {
      Serial.printf("  BEACON from %s - Room: %s, User: %s\n",
                    beacon->deviceID, beacon->roomName, beacon->username);

      // Add to ESP-NOW peer list for radar tracking (NO announcement - beacons are for discovery only)
      espNowManager.addPeer(mac, beacon->deviceID, beacon->username, beacon->roomName, false);

      return true;
    }
  }

  // Check if it's a public knock (unencrypted)
  if (len == sizeof(PublicKnock)) {
    const PublicKnock* knock = (const PublicKnock*)data;
    if (knock->type == MSG_KNOCK && knock->version == PROTOCOL_VERSION) {
      Serial.printf("  PUBLIC KNOCK from %s to room: %s\n",
                    knock->fromUsername, knock->toRoomName);

      // Only show if knock is for OUR room
      const char* ourRoom = securityManager.isConnected() ?
        securityManager.getNetworkName() : "";

      if (strcmp(knock->toRoomName, ourRoom) == 0) {
        // Store knocker info
        extern String knockerDeviceID;
        extern String knockerUsername;
        knockerDeviceID = String(knock->fromDeviceID);
        knockerUsername = String(knock->fromUsername);

        String knockMsg = String(knock->fromUsername) + " is knocking, type allow";
        addSystemMessage(knockMsg.c_str());
      }

      return true;
    }
  }

  // Check for public knock response (unencrypted)
  if (len == sizeof(PublicKnockResponse)) {
    const PublicKnockResponse* response = (const PublicKnockResponse*)data;
    if (response->type == MSG_KNOCK_RESPONSE && response->version == PROTOCOL_VERSION) {
      Serial.printf("  PUBLIC KNOCK RESPONSE from %s to %s: %s\n",
                    response->fromDeviceID, response->toDeviceID,
                    response->allowed ? "ALLOWED" : "DENIED");

      // Only process if it's for US
      if (strcmp(response->toDeviceID, securityManager.getDeviceID()) == 0) {
        extern LabChatState chatState;
        extern String lobbyRoomName;
        extern unsigned long lastPresenceBroadcast;
        extern unsigned long lobbyKnockTime;

        if (response->allowed && strlen(response->roomPassword) > 0) {
          Serial.printf("  Allowed with password (len=%d)\n", strlen(response->roomPassword));

          if (chatState == CHAT_LOBBY && lobbyRoomName.length() > 0) {
            // Deinit ESP-NOW first (clear temp PMK)
            Serial.println("  Deinitializing ESP-NOW...");
            espNowManager.deinit();

            // Join the network with provided password
            String roomPassword = String(response->roomPassword);
            Serial.printf("  Attempting to join network...\n");

            if (securityManager.joinNetwork(roomPassword)) {
              Serial.println("  SUCCESS: Network joined!");

              // Store password in activeRooms so we can allow others later
              extern int activeRoomCount;
              extern int currentRoomIndex;
              extern RoomInfo activeRooms[];

              String newNetwork = String(securityManager.getNetworkName());
              bool roomExists = false;
              for (int i = 0; i < activeRoomCount; i++) {
                if (activeRooms[i].password == roomPassword) {
                  roomExists = true;
                  currentRoomIndex = i;
                  break;
                }
              }
              if (!roomExists && activeRoomCount < 10) { // MAX_ROOMS
                activeRooms[activeRoomCount].name = newNetwork;
                activeRooms[activeRoomCount].password = roomPassword;
                currentRoomIndex = activeRoomCount;
                activeRoomCount++;
                Serial.printf("  Stored room in activeRooms[%d]\n", currentRoomIndex);
              }

              // Reinitialize ESP-NOW with proper room key
              espNowManager.init(securityManager.getPMK());

              // Transition to chat
              chatState = CHAT_MAIN;
              sendPresence();
              lastPresenceBroadcast = millis();
              lobbyRoomName = "";
              lobbyKnockTime = 0;

              // Show welcome message
              String welcomeMsg = "You have been allowed into the room!";
              addSystemMessage(welcomeMsg.c_str(), 0);
            } else {
              Serial.println("  ERROR: Failed to join network!");

              // Reinit ESP-NOW with temp PMK to continue receiving
              uint8_t tempPMK[16] = {0};
              espNowManager.init(tempPMK);

              String errorMsg = "Failed to join room";
              addSystemMessage(errorMsg.c_str(), 0);
            }
          }
        } else if (!response->allowed) {
          // Denied
          String denyMsg = "Access denied";
          addSystemMessage(denyMsg.c_str(), 0);
        }
      }

      return true;
    }
  }

  // Otherwise, expect encrypted message
  if (len != sizeof(SecureMessage)) {
    Serial.printf("  ERROR: Length mismatch! Got %d, expected %d or %d\n",
                  len, sizeof(SecureMessage), sizeof(DiscoveryBeacon));
    return false;
  }

  const SecureMessage* msg = (const SecureMessage*)data;

  // Verify message
  Serial.println("  Verifying message...");
  if (!verifyMessage(msg)) {
    Serial.println("  ERROR: Message verification failed!");
    return false;
  }
  Serial.println("  Message verified successfully");

  // Ignore our own messages
  if (strcmp(msg->deviceID, securityManager.getDeviceID()) == 0) {
    Serial.println("  Ignoring own message");
    return true;
  }

  // Handle presence announcements
  if (msg->type == MSG_PRESENCE) {
    Serial.printf("  PRESENCE message from %s (%s)\n", msg->username, msg->deviceID);
    Serial.printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    espNowManager.addPeer(mac, msg->deviceID, msg->username);
    return true;
  }

  // Handle knock requests
  if (msg->type == MSG_KNOCK) {
    Serial.printf("  KNOCK message from %s (%s)\n", msg->username, msg->deviceID);
    espNowManager.addPeer(mac, msg->deviceID, msg->username);

    // Add knock notification to chat
    String knockMsg = String(msg->username) + " is knocking. Type 'allow' to admit.";
    addSystemMessage(knockMsg.c_str(), 0);
    return true;
  }

  // Note: Knock responses are now handled as unencrypted PublicKnockResponse above
  // This encrypted handler is no longer used but kept for potential future use

  // Filter by message type
  if (msg->type == MSG_DIRECT) {
    // Check if message is for us
    if (strcmp(msg->targetID, securityManager.getDeviceID()) != 0) {
      return true; // Not for us, ignore silently
    }
  }

  // Add peer if not already known
  espNowManager.addPeer(mac, msg->deviceID, msg->username);

  // Filter out emoji chunk messages - DON'T add to queue, process via callback
  String content = String(msg->payload);
  if (content.startsWith("EMOJI:")) {
    Serial.println("Received emoji chunk - processing via callback");
    // Call the emoji chunk processing callback if set
    if (emojiChunkCallback) {
      emojiChunkCallback(content);
    }
    return true; // Successfully received but don't add to display queue
  }

  // Check if message contains custom emoji shortcuts
  // If so, delay adding to queue to give chunks time to arrive
  bool hasCustomEmoji = false;
  int colonPos = content.indexOf(':');
  if (colonPos >= 0 && colonPos < content.length() - 1) {
    int endColon = content.indexOf(':', colonPos + 1);
    if (endColon > colonPos + 1) {
      hasCustomEmoji = true;
    }
  }

  if (hasCustomEmoji) {
    // Small delay to let emoji chunks arrive (8 chunks * 50ms = ~400ms)
    delay(450);
  }

  // Create display message
  DisplayMessage displayMsg;
  strncpy(displayMsg.deviceID, msg->deviceID, 15);
  displayMsg.deviceID[15] = '\0';
  strncpy(displayMsg.username, msg->username, 15);
  displayMsg.username[15] = '\0';
  displayMsg.channel = msg->channel;
  displayMsg.type = (MessageType)msg->type;
  displayMsg.timestamp = msg->timestamp;
  displayMsg.content = content;
  displayMsg.isOwn = false;

  // Add to queue
  return addToQueue(displayMsg);
}

bool MessageHandler::addToQueue(const DisplayMessage& msg) {
  // Add to circular buffer
  messageQueue[queueHead] = msg;
  queueHead = (queueHead + 1) % MESSAGE_QUEUE_SIZE;

  if (queueCount < MESSAGE_QUEUE_SIZE) {
    queueCount++;
  } else {
    // Queue full, move tail forward
    queueTail = (queueTail + 1) % MESSAGE_QUEUE_SIZE;
  }

  // Trigger display update callback if set
  if (onMessageAddedCallback) {
    onMessageAddedCallback();
  }

  return true;
}

DisplayMessage* MessageHandler::getQueuedMessage(int index) {
  if (index < 0 || index >= queueCount) return nullptr;

  int actualIndex = (queueTail + index) % MESSAGE_QUEUE_SIZE;
  return &messageQueue[actualIndex];
}

void MessageHandler::clearQueue() {
  queueHead = 0;
  queueTail = 0;
  queueCount = 0;
  memset(messageQueue, 0, sizeof(messageQueue));
}

bool MessageHandler::sendBroadcast(const char* content, uint8_t channel) {
  Serial.printf("MessageHandler::sendBroadcast() - Content: \"%s\", Channel: %d\n", content, channel);

  SecureMessage msg;
  if (!createMessage(MSG_BROADCAST, content, channel, nullptr, &msg)) {
    Serial.println("  ERROR: createMessage failed!");
    return false;
  }

  Serial.printf("  Message created - Type: %d, DeviceID: %s, Username: %s\n",
                msg.type, msg.deviceID, msg.username);
  Serial.printf("  Payload: \"%s\", Length: %d\n", msg.payload, msg.msgLen);

  // Add to our own queue for display (but not emoji chunks)
  String msgPayload = String(msg.payload);
  if (!msgPayload.startsWith("EMOJI:")) {
    DisplayMessage displayMsg;
    strncpy(displayMsg.deviceID, msg.deviceID, 15);
    strncpy(displayMsg.username, msg.username, 15);
    displayMsg.channel = msg.channel;
    displayMsg.type = MSG_BROADCAST;
    displayMsg.timestamp = msg.timestamp;
    displayMsg.content = msgPayload;
    displayMsg.isOwn = true;
    addToQueue(displayMsg);
  }

  // Send via ESP-NOW
  return espNowManager.sendBroadcast((uint8_t*)&msg, sizeof(msg));
}

bool MessageHandler::sendDirect(const char* targetID, const char* content) {
  SecureMessage msg;
  if (!createMessage(MSG_DIRECT, content, 0, targetID, &msg)) {
    return false;
  }

  // Add to our own queue (but not emoji chunks)
  String msgContent = String(msg.payload);
  if (!msgContent.startsWith("EMOJI:")) {
    DisplayMessage displayMsg;
    strncpy(displayMsg.deviceID, msg.deviceID, 15);
    strncpy(displayMsg.username, msg.username, 15);
    displayMsg.channel = 0;
    displayMsg.type = MSG_DIRECT;
    displayMsg.timestamp = msg.timestamp;
    displayMsg.content = String("→ ") + String(targetID) + ": " + msgContent;
    displayMsg.isOwn = true;
    addToQueue(displayMsg);
  }

  // Send to specific device
  return espNowManager.sendToDeviceID(targetID, (uint8_t*)&msg, sizeof(msg));
}

bool MessageHandler::sendPresence() {
  SecureMessage msg;
  if (!createMessage(MSG_PRESENCE, "online", 0, nullptr, &msg)) {
    return false;
  }

  return espNowManager.sendBroadcast((uint8_t*)&msg, sizeof(msg));
}

bool MessageHandler::sendBeacon() {
  DiscoveryBeacon beacon;
  memset(&beacon, 0, sizeof(beacon));

  beacon.type = MSG_BEACON;
  beacon.version = PROTOCOL_VERSION;

  // Device ID
  strncpy(beacon.deviceID, securityManager.getDeviceID(), 15);
  beacon.deviceID[15] = '\0';

  // Username (load from preferences)
  Preferences prefs;
  prefs.begin("labchat", true);
  String defaultUsername = "User";
  const char* devID = securityManager.getDeviceID();
  int userNum = 0;
  int len = strlen(devID);
  if (len >= 4) {
    for (int i = len - 4; i < len; i++) {
      userNum = userNum * 10 + (devID[i] % 10);
    }
  }
  defaultUsername += String(userNum % 10000);
  String username = prefs.getString("username", defaultUsername);
  prefs.end();
  strncpy(beacon.username, username.c_str(), 15);
  beacon.username[15] = '\0';

  // Room name (from security manager)
  const char* roomName = securityManager.isConnected() ?
    securityManager.getNetworkName() : "No Room";
  strncpy(beacon.roomName, roomName, 31);
  beacon.roomName[31] = '\0';

  Serial.printf("Broadcasting beacon: Room='%s', User='%s'\n",
                beacon.roomName, beacon.username);

  // Broadcast unencrypted
  return espNowManager.sendBroadcast((uint8_t*)&beacon, sizeof(beacon));
}

bool MessageHandler::sendPublicKnock(const char* targetRoomName) {
  PublicKnock knock;
  memset(&knock, 0, sizeof(knock));

  knock.type = MSG_KNOCK;
  knock.version = PROTOCOL_VERSION;

  // Sender info
  strncpy(knock.fromDeviceID, securityManager.getDeviceID(), 15);
  knock.fromDeviceID[15] = '\0';

  // Username
  Preferences prefs;
  prefs.begin("labchat", true);
  String defaultUsername = "User";
  const char* devID = securityManager.getDeviceID();
  int userNum = 0;
  int len = strlen(devID);
  if (len >= 4) {
    for (int i = len - 4; i < len; i++) {
      userNum = userNum * 10 + (devID[i] % 10);
    }
  }
  defaultUsername += String(userNum % 10000);
  String username = prefs.getString("username", defaultUsername);
  prefs.end();
  strncpy(knock.fromUsername, username.c_str(), 15);
  knock.fromUsername[15] = '\0';

  // Target room
  strncpy(knock.toRoomName, targetRoomName, 31);
  knock.toRoomName[31] = '\0';

  Serial.printf("Sending public knock to room: %s from %s\n", targetRoomName, username.c_str());

  // Broadcast unencrypted
  return espNowManager.sendBroadcast((uint8_t*)&knock, sizeof(knock));
}

bool MessageHandler::sendKnock(const char* targetID) {
  SecureMessage msg;
  if (!createMessage(MSG_KNOCK, "knock", 0, targetID, &msg)) {
    return false;
  }

  return espNowManager.sendToDeviceID(targetID, (uint8_t*)&msg, sizeof(msg));
}

bool MessageHandler::sendKnockResponse(const char* targetID, bool allowed, const char* roomPassword) {
  // Send unencrypted response (knocker doesn't have room key yet)
  PublicKnockResponse response;
  memset(&response, 0, sizeof(response));

  response.type = MSG_KNOCK_RESPONSE;
  response.version = PROTOCOL_VERSION;

  // Sender info
  strncpy(response.fromDeviceID, securityManager.getDeviceID(), 15);
  response.fromDeviceID[15] = '\0';

  // Target info
  strncpy(response.toDeviceID, targetID, 15);
  response.toDeviceID[15] = '\0';

  // Allow/deny
  response.allowed = allowed ? 1 : 0;

  // Room password (if allowed)
  if (allowed && roomPassword != nullptr && strlen(roomPassword) > 0) {
    strncpy(response.roomPassword, roomPassword, 63);
    response.roomPassword[63] = '\0';
    Serial.printf("Sending knock response: ALLOWED with password (len=%d)\n", strlen(roomPassword));
  } else {
    Serial.printf("Sending knock response: %s\n", allowed ? "ALLOWED (no pass)" : "DENIED");
  }

  // Broadcast unencrypted (knocker will filter by toDeviceID)
  return espNowManager.sendBroadcast((uint8_t*)&response, sizeof(response));
}

void MessageHandler::addSystemMessage(const char* message, uint8_t channel) {
  DisplayMessage displayMsg;
  strncpy(displayMsg.deviceID, "SYSTEM", 15);
  strncpy(displayMsg.username, "System", 15);
  displayMsg.channel = channel;
  displayMsg.type = MSG_SYSTEM;
  displayMsg.timestamp = millis();
  displayMsg.content = String(message);
  displayMsg.isOwn = false;
  addToQueue(displayMsg);
}

bool MessageHandler::sendEmojiFile(const char* targetID, const char* filename) {
  // Load emoji file from SD card
  if (!SD.begin()) {
    Serial.println("SD card not available for emoji send");
    return false;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.print("Failed to open emoji file: ");
    Serial.println(filename);
    return false;
  }

  // Read emoji pixel data (512 bytes = 16x16 uint16_t)
  uint8_t pixelData[512];
  size_t bytesRead = file.read(pixelData, 512);
  file.close();

  if (bytesRead != 512) {
    Serial.println("Failed to read complete emoji file");
    return false;
  }

  // Extract shortcut name from filename
  String path = String(filename);
  int lastSlash = path.lastIndexOf('/');
  String shortcut = path.substring(lastSlash + 1);
  shortcut.replace(".emoji", "");

  Serial.print("Sending emoji file to ");
  Serial.print(targetID);
  Serial.print(": ");
  Serial.println(shortcut);

  // Send in 8 chunks of 64 bytes each (128 hex chars per chunk)
  for (int chunk = 0; chunk < 8; chunk++) {
    String chunkData = "EMOJI:" + shortcut + ":" + String(chunk) + ":";

    // Encode 64 bytes as hex (128 chars)
    for (int b = chunk * 64; b < (chunk + 1) * 64 && b < 512; b++) {
      char hex[3];
      sprintf(hex, "%02X", pixelData[b]);
      chunkData += hex;
    }

    // Send chunk as direct message
    if (!sendDirect(targetID, chunkData.c_str())) {
      Serial.print("Failed to send chunk ");
      Serial.println(chunk);
      return false;
    }

    delay(250); // Longer delay to prevent buffer overflow
  }

  Serial.println("Emoji file sent successfully");
  return true;
}

bool MessageHandler::sendEmojiFileBroadcast(const char* filename) {
  // Load emoji file from SD card
  if (!SD.begin()) {
    Serial.println("SD card not available for emoji broadcast");
    return false;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.print("Failed to open emoji file: ");
    Serial.println(filename);
    return false;
  }

  // Read emoji pixel data (512 bytes = 16x16 uint16_t)
  uint8_t pixelData[512];
  size_t bytesRead = file.read(pixelData, 512);
  file.close();

  if (bytesRead != 512) {
    Serial.println("Failed to read complete emoji file");
    return false;
  }

  // Extract shortcut name from filename
  String path = String(filename);
  int lastSlash = path.lastIndexOf('/');
  String shortcut = path.substring(lastSlash + 1);
  shortcut.replace(".emoji", "");

  Serial.print("Broadcasting emoji file: ");
  Serial.println(shortcut);

  // Send in 8 chunks of 64 bytes each (128 hex chars per chunk)
  for (int chunk = 0; chunk < 8; chunk++) {
    String chunkData = "EMOJI:" + shortcut + ":" + String(chunk) + ":";

    // Encode 64 bytes as hex (128 chars)
    for (int b = chunk * 64; b < (chunk + 1) * 64 && b < 512; b++) {
      char hex[3];
      sprintf(hex, "%02X", pixelData[b]);
      chunkData += hex;
    }

    // Send chunk as broadcast on channel 0
    if (!sendBroadcast(chunkData.c_str(), 0)) {
      Serial.print("Failed to broadcast chunk ");
      Serial.println(chunk);
      return false;
    }

    delay(250); // Longer delay to prevent buffer overflow
  }

  Serial.println("Emoji file broadcast successfully");
  return true;
}

// ESP-NOW callback implementation
void onMessageReceived(const uint8_t* mac, const uint8_t* data, int len) {
  messageHandler.handleReceivedMessage(mac, data, len);
}
