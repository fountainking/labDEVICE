#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <Arduino.h>

// Message types
enum MessageType : uint8_t {
  MSG_BROADCAST = 0x01,
  MSG_DIRECT = 0x02,
  MSG_CHANNEL = 0x03,
  MSG_SYSTEM = 0x04,
  MSG_PRESENCE = 0x05,
  MSG_FILE_START = 0x06,    // Start of file transfer (contains filename + total size)
  MSG_FILE_CHUNK = 0x07,    // File data chunk
  MSG_FILE_END = 0x08       // End of file transfer
};

// Protocol version
#define PROTOCOL_VERSION 1

// Message structure (exactly 250 bytes - ESP-NOW limit)
struct SecureMessage {
  uint8_t type;           // 1 byte: message type
  uint8_t version;        // 1 byte: protocol version
  uint32_t timestamp;     // 4 bytes: unix timestamp
  char deviceID[16];      // 16 bytes: sender device ID
  char username[16];      // 16 bytes: sender username
  uint8_t channel;        // 1 byte: channel number (0-9)
  char targetID[16];      // 16 bytes: target device ID (for DM)
  uint16_t msgLen;        // 2 bytes: payload length
  char payload[161];      // 161 bytes: actual message content (was 165, reduced to fit 250 byte limit)
  uint8_t hmac[32];       // 32 bytes: HMAC-SHA256
  // Total: 1+1+4+16+16+1+16+2+161+32 = 250 bytes exactly
} __attribute__((packed));

// Display message (for UI)
struct DisplayMessage {
  char deviceID[16];
  char username[16];
  uint8_t channel;
  MessageType type;
  unsigned long timestamp;
  String content;
  bool isOwn;
};

// Message queue for display
#define MESSAGE_QUEUE_SIZE 50

class MessageHandler {
private:
  DisplayMessage messageQueue[MESSAGE_QUEUE_SIZE];
  int queueHead;
  int queueTail;
  int queueCount;

  bool verifyMessage(const SecureMessage* msg);

  // Callback for display updates
  void (*onMessageAddedCallback)();

  // File transfer state
  String receivingFilename;
  uint8_t* receiveBuffer;
  size_t receiveBufferSize;
  size_t receivedBytes;
  bool isReceivingFile;

public:
  MessageHandler();

  // Message creation
  bool createMessage(MessageType type, const char* content, uint8_t channel,
                     const char* targetID, SecureMessage* outMsg);

  // Message handling (called by ESP-NOW callback)
  bool handleReceivedMessage(const uint8_t* mac, const uint8_t* data, int len);

  // Queue management
  bool addToQueue(const DisplayMessage& msg);
  DisplayMessage* getQueuedMessage(int index);
  int getQueueCount() { return queueCount; }
  void clearQueue();

  // Sending helpers
  bool sendBroadcast(const char* content, uint8_t channel = 0);
  bool sendDirect(const char* targetID, const char* content);
  bool sendPresence(); // Announce presence to network

  // File transfer
  bool sendEmojiFile(const char* targetID, const char* filename); // Send .emoji file to specific user
  bool sendEmojiFileBroadcast(const char* filename); // Broadcast .emoji file to all

  // Local system messages (not sent over network)
  void addSystemMessage(const char* message, uint8_t channel = 0);

  // Set callback for real-time display updates
  void setMessageCallback(void (*callback)()) { onMessageAddedCallback = callback; }

  // Set callback for emoji chunk processing
  void (*emojiChunkCallback)(const String& content);
  void setEmojiChunkCallback(void (*callback)(const String&)) { emojiChunkCallback = callback; }
};

extern MessageHandler messageHandler;

// ESP-NOW callback implementation
void onMessageReceived(const uint8_t* mac, const uint8_t* data, int len);

#endif
