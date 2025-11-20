#include "labchat.h"
#include "security_manager.h"
#include "esp_now_manager.h"
#include "message_handler.h"
#include "ui.h"
#include "audio_manager.h"
#include "file_manager.h"
#include "radio.h"
#include <M5Cardputer.h>
#include <SD.h>

// Star emoji - hardcoded from star.emoji (centered for title bar/WiFi icons)
static const uint16_t STAR_ICON[16][16] = {
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x0000, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0x0000, 0x07E0, 0x07E0},
    {0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0},
    {0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0},
    {0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x0000, 0xFFE0, 0xFFE0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x0000, 0x0000, 0xFFE0, 0xFFE0, 0x0000, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
};

// Berry emoji - hardcoded from berry.emoji
static const uint16_t BERRY_ICON[16][16] = {
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x03E0, 0x03E0, 0x07E0, 0x07E0, 0x03E0, 0x03E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0x03E0, 0x03E0, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xFFE0, 0xF800, 0xFFE0, 0xF800, 0xFFE0, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xFFE0, 0xF800, 0xFFE0, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0xF800, 0xF800, 0xF800, 0xF800, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
    {0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x07E0},
};

// System emoji data structure
struct SystemEmoji {
  String shortcut;              // :name:
  uint16_t pixels[16][16];      // 16x16 pixel data
};

// System emoji storage (20 max, slot 0 = strawberry)
static SystemEmoji systemEmojis[20];
static int systemEmojiCount = 0;
static bool systemEmojisLoaded = false;

// Friend emoji storage (12 max, received from other users)
static SystemEmoji friendEmojis[12];
static int friendEmojiCount = 0;
static bool showFriendEmojis = false; // Toggle for emoji picker view

// Emoji reception buffer for multi-chunk transfers
struct EmojiReceiveBuffer {
  String shortcut;
  uint8_t pixelData[512];
  bool chunksReceived[8]; // Track which chunks we've received (8 chunks of 64 bytes)
  bool isComplete;
};
static EmojiReceiveBuffer emojiReceiveBuffer;
static bool receivingEmoji = false;

// Track which emojis have been sent to which peers to avoid redundant transfers
struct EmojiSentRecord {
  String shortcut;
  String peerDeviceIDs[10]; // Track up to 10 peers who have this emoji
  int peerCount;
};
static EmojiSentRecord sentEmojiHistory[32]; // Track 32 recent sends (system + friend)
static int sentEmojiHistoryCount = 0;

// Message buffering for messages with pending emoji transfers
struct BufferedMessage {
  String content;
  String deviceID;
  char username[16];
  int channel;
  MessageType type;
  unsigned long timestamp;
  String pendingEmojis[5]; // Track up to 5 emojis we're waiting for
  int pendingEmojiCount;
};
static BufferedMessage bufferedMessages[10];
static int bufferedMessageCount = 0;

// LabCHAT state
LabChatState chatState = CHAT_SETUP_PIN;
String pinInput = "";
String networkPasswordInput = "";
String networkNameInput = "";
String chatInput = "";
String usernameInput = "";
String channelNameInput = "";
int scrollPosition = 0;
int selectedUserIndex = 0;
int selectedEmojiIndex = 0;
int selectedEmojiManagerIndex = 0;  // For emoji manager navigation
int chatCurrentChannel = 0;
bool chatActive = false;
unsigned long lastPresenceBroadcast = 0;
String dmTargetID = "";
String dmTargetUsername = "";
String lobbyRoomName = "";  // Room name for lobby (after knocking)
unsigned long lobbyKnockTime = 0;  // Timestamp when knock was sent
String knockerDeviceID = "";  // Device ID of last person who knocked
String knockerUsername = "";  // Username of last person who knocked
bool hasUnreadMessages = false;
String lastNetworkName = "";  // Track previous network for message persistence

// Interrupt-safe flags for callback → main loop communication
volatile bool needsRedraw = false;
volatile bool needsBeep = false;

// Channel names
String channelNames[10] = {
  "general", "random", "dev", "music",
  "games", "tech", "art", "memes",
  "study", "chill"
};

// Room management (temporary, cleared on power off)
#define MAX_ROOMS 10
RoomInfo activeRooms[MAX_ROOMS];
int activeRoomCount = 0;
int currentRoomIndex = -1;

// Menu indices
int networkMenuIndex = 0;
int chatSettingsMenuIndex = 0;

// Room Radar globals
struct NearbyDevice {
  String deviceID;
  String roomName;
  int rssi;
  unsigned long lastSeen;
};
#define MAX_NEARBY_DEVICES 10
NearbyDevice nearbyDevices[MAX_NEARBY_DEVICES];
int nearbyDeviceCount = 0;
int selectedRadarIndex = 0;
bool radarTrackingMode = false;

// Helper function to extract color from network name (strips " Room" suffix)
String getColorFromNetworkName(const String& networkName) {
  String color = networkName;
  if (color.endsWith(" Room")) {
    color = color.substring(0, color.length() - 5);
  }
  return color;
}
unsigned long lastRadarScan = 0;

// Cursor blink
bool cursorVisible = true;
unsigned long lastCursorBlink = 0;

// Transparency color (lime green) - matches emoji_maker.cpp
#define TRANSPARENCY_COLOR 0x07E0

// Forward declarations of shared functions
extern void drawStar(int x, int y, int size, uint16_t color);
extern void drawNavHint(const char* text, int x, int y);
extern void drawEmojiIcon(int x, int y, const char* emoji, uint16_t color, int size);
extern uint16_t interpolateColor(uint16_t color1, uint16_t color2, float t);
void drawCustomEmoji(int screenX, int screenY, int emojiIndex, int scale); // Forward declaration
void saveFriendEmoji(const String& shortcut, const uint16_t pixels[16][16]); // Forward declaration
void loadSystemEmojis(); // Forward declaration

// Helper to check if emoji was already sent to a peer
bool wasEmojiSentToPeer(const String& shortcut, const String& peerDeviceID) {
  for (int i = 0; i < sentEmojiHistoryCount; i++) {
    if (sentEmojiHistory[i].shortcut == shortcut) {
      // Found this emoji - check if peer has it
      for (int p = 0; p < sentEmojiHistory[i].peerCount; p++) {
        if (sentEmojiHistory[i].peerDeviceIDs[p] == peerDeviceID) {
          return true; // Already sent to this peer
        }
      }
    }
  }
  return false; // Not sent yet
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Helper to mark emoji as sent to a peer
void markEmojiSentToPeer(const String& shortcut, const String& peerDeviceID) {
  // Find or create record for this emoji
  int recordIndex = -1;
  for (int i = 0; i < sentEmojiHistoryCount; i++) {
    if (sentEmojiHistory[i].shortcut == shortcut) {
      recordIndex = i;
      break;
    }
  }

  if (recordIndex == -1) {
    // Create new record
    if (sentEmojiHistoryCount < 32) {
      recordIndex = sentEmojiHistoryCount++;
      sentEmojiHistory[recordIndex].shortcut = shortcut;
      sentEmojiHistory[recordIndex].peerCount = 0;
    } else {
      return; // History full
    }
  }

  // Add peer to this emoji's record (if not already there)
  EmojiSentRecord* record = &sentEmojiHistory[recordIndex];
  for (int i = 0; i < record->peerCount; i++) {
    if (record->peerDeviceIDs[i] == peerDeviceID) {
      return; // Already tracked
    }
  }

  if (record->peerCount < 10) {
    record->peerDeviceIDs[record->peerCount++] = peerDeviceID;
  }
}

// Helper to process incoming emoji chunk messages
void processEmojiChunk(const String& content) {
  // Format: "EMOJI:shortcut:chunkNum:HEXDATA"
  if (!content.startsWith("EMOJI:")) return;

  // Safety check - don't accept emoji if slots are full
  if (friendEmojiCount >= 12) {
    Serial.println("⚠️ Friend emoji slots full, rejecting chunk");
    return;
  }

  int firstColon = content.indexOf(':', 6);
  if (firstColon == -1) return;

  int secondColon = content.indexOf(':', firstColon + 1);
  if (secondColon == -1) return;

  String shortcut = content.substring(6, firstColon);
  int chunkNum = content.substring(firstColon + 1, secondColon).toInt();
  String hexData = content.substring(secondColon + 1);

  Serial.println("================================================");
  Serial.print("📥 RECEIVING CHUNK ");
  Serial.print(chunkNum);
  Serial.print("/8 for emoji: ");
  Serial.println(shortcut);
  Serial.print("   Data length: ");
  Serial.print(hexData.length());
  Serial.println(" chars");

  // Initialize receive buffer if this is a new emoji
  if (!receivingEmoji || emojiReceiveBuffer.shortcut != shortcut) {
    receivingEmoji = true;
    emojiReceiveBuffer.shortcut = shortcut;
    emojiReceiveBuffer.isComplete = false;
    memset(emojiReceiveBuffer.pixelData, 0, 512);
    for (int i = 0; i < 8; i++) {
      emojiReceiveBuffer.chunksReceived[i] = false;
    }
    Serial.println("Started receiving new emoji: " + shortcut);
  }

  // Decode hex data into pixel buffer (8 chunks of 64 bytes each)
  if (chunkNum >= 0 && chunkNum < 8 && !emojiReceiveBuffer.chunksReceived[chunkNum]) {
    int offset = chunkNum * 64;
    Serial.print("  Decoding chunk ");
    Serial.print(chunkNum);
    Serial.print(" - hexData length: ");
    Serial.print(hexData.length());
    Serial.print(" - offset: ");
    Serial.println(offset);

    // Each chunk is 128 hex chars = 64 bytes
    for (int i = 0; i < hexData.length() && i < 128; i += 2) {
      String byteStr = hexData.substring(i, i + 2);
      uint8_t byteVal = strtol(byteStr.c_str(), NULL, 16);
      emojiReceiveBuffer.pixelData[offset + i/2] = byteVal;
    }
    emojiReceiveBuffer.chunksReceived[chunkNum] = true;
    Serial.print("  Chunk ");
    Serial.print(chunkNum);
    Serial.println(" decoded successfully");

    // Check if all chunks received
    bool allReceived = true;
    Serial.print("  Chunks status: [");
    for (int i = 0; i < 8; i++) {
      Serial.print(emojiReceiveBuffer.chunksReceived[i] ? "✓" : "✗");
      if (!emojiReceiveBuffer.chunksReceived[i]) {
        allReceived = false;
      }
    }
    Serial.println("]");

    if (allReceived) {
      // Save complete emoji
      Serial.println("================================================");
      Serial.println("✅ ALL 8 CHUNKS RECEIVED SUCCESSFULLY!");
      Serial.println("   Assembling emoji: " + shortcut);
      emojiReceiveBuffer.isComplete = true;
      uint16_t (*pixels)[16] = (uint16_t (*)[16])emojiReceiveBuffer.pixelData;
      saveFriendEmoji(shortcut, pixels);
      receivingEmoji = false;
      Serial.print("   ✓ Saved friend emoji: ");
      Serial.print(shortcut);
      Serial.print(" (total friend emojis: ");
      Serial.print(friendEmojiCount);
      Serial.println("/12)");
      Serial.println("   Ready to display in messages!");
      Serial.println("================================================");

      // Trigger a single redraw now that emoji is ready
      if (chatActive && chatState == CHAT_MAIN) {
        needsRedraw = true;
      }
    }
  }
}

// Helper to render text with embedded emojis
// Returns the x position after rendering
int drawTextWithEmojis(const char* text, int startX, int y, uint16_t textColor) {
  int xPos = startX;
  int len = strlen(text);
  int i = 0;

  while (i < len) {
    bool handled = false;

    // Check for :shortcut: pattern for custom emojis
    if (text[i] == ':') {
      int endColon = -1;
      for (int j = i + 1; j < len && j < i + 20; j++) { // Max shortcut length 20
        if (text[j] == ':') {
          endColon = j;
          break;
        }
      }

      if (endColon > i + 1) {
        // Extract shortcut
        String shortcut = "";
        for (int j = i + 1; j < endColon; j++) {
          shortcut += text[j];
        }

        // Find matching system emoji
        bool found = false;
        for (int e = 0; e < systemEmojiCount; e++) {
          if (systemEmojis[e].shortcut == shortcut) {
            // Draw custom emoji at 1x scale, bottom-aligned with text
            // Text is 8px tall, emoji is 16px, so shift up by 8px
            drawCustomEmoji(xPos, y - 8, e, 1);
            xPos += 16; // 16 pixels wide
            i = endColon + 1; // Skip past :shortcut:
            handled = true;
            found = true;
            break;
          }
        }

        // If not found in system emojis, check friend emojis
        if (!found) {
          Serial.print("Looking for '");
          Serial.print(shortcut);
          Serial.print("' in ");
          Serial.print(friendEmojiCount);
          Serial.println(" friend emojis");

          for (int e = 0; e < friendEmojiCount; e++) {
            Serial.print("  Checking against: ");
            Serial.println(friendEmojis[e].shortcut);

            if (friendEmojis[e].shortcut == shortcut) {
              Serial.println("  MATCH! Rendering friend emoji");
              // Draw friend emoji at 1x scale, bottom-aligned with text
              // Text is 8px tall, emoji is 16px, so shift up by 8px
              for (int py = 0; py < 16; py++) {
                for (int px = 0; px < 16; px++) {
                  uint16_t color = friendEmojis[e].pixels[py][px];
                  if (color != TRANSPARENCY_COLOR) {
                    canvas.drawPixel(xPos + px, y - 8 + py, color);
                  }
                }
              }
              xPos += 16; // 16 pixels wide
              i = endColon + 1; // Skip past :shortcut:
              handled = true;
              found = true;
              break;
            }
          }

          if (!found) {
            Serial.print("  NOT FOUND in friend emojis: ");
            Serial.println(shortcut);
          }
        }
      }
    }

    // Regular ASCII character
    if (!handled) {
      canvas.setTextSize(1);
      canvas.setTextColor(textColor);
      canvas.drawChar(text[i], xPos, y);
      xPos += 6;
      i++;
    }
  }

  return xPos;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void drawLabChatHeader(const char* subtitle) {
  // Calculate header width
  String headerText = "LabCHAT";
  if (subtitle) {
    headerText += " - ";
    headerText += subtitle;
  }

  int textWidth = headerText.length() * 6;
  int totalWidth = 20 + textWidth + 30; // Emoji + text + padding

  // Header rectangle (left aligned at x=5)
  canvas.fillRoundRect(5, 8, totalWidth, 20, 10, TFT_WHITE);
  canvas.drawRoundRect(5, 8, totalWidth, 20, 10, TFT_BLACK);

  // Star emoji ⭐
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      uint16_t color = STAR_ICON[y][x];
      if (color != 0x07E0) { // Skip transparent pixels
        canvas.drawPixel(11 + x, 10 + y, color);
      }
    }
  }

  // "LabCHAT" text
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("LabCHAT", 29, 14);

  // Subtitle (no gradient on header)
  if (subtitle) {
    canvas.drawString(" - ", 29 + 42, 14);
    canvas.drawString(subtitle, 29 + 42 + 18, 14);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawTextInputBox(const char* prompt, String& input, bool isPassword) {
  // Input box (Files aesthetic)
  canvas.fillRoundRect(20, 50, 200, 60, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 50, 200, 60, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 51, 198, 58, 11, TFT_BLACK);

  // Prompt
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString(prompt, 30, 60);

  // Input field
  String displayText = input;
  if (isPassword && input.length() > 0) {
    displayText = "";
    for (int i = 0; i < input.length(); i++) {
      displayText += "*";
    }
  }

  if (displayText.length() > 28) {
    displayText = displayText.substring(displayText.length() - 28);
  }

  canvas.drawString(displayText.c_str(), 30, 75);

  // Blinking cursor
  if (cursorVisible) {
    int cursorX = 30 + (displayText.length() * 6);
    canvas.drawLine(cursorX, 75, cursorX, 83, TFT_BLACK);
  }

  // Nav hints
  drawNavHint("Enter=OK  Del=Erase  `=Back", 40, 95);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// ============================================================================
// STATE DRAWING
// ============================================================================

void drawPinSetup() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Setup");

  drawTextInputBox("Create 4-char PIN:", pinInput, true);

  // Info
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Use 0-9, A-Z, !@#$%", 45, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawPinEntry() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Unlock");

  drawTextInputBox("Enter PIN:", pinInput, true);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawNetworkMenu() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Network");

  // Menu box
  canvas.fillRoundRect(20, 40, 200, 70, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 40, 200, 70, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 41, 198, 68, 11, TFT_BLACK);

  const char* options[] = {"Create Network", "Join Network"};

  for (int i = 0; i < 2; i++) {
    canvas.setTextSize(1);
    if (i == networkMenuIndex) {
      canvas.setTextColor(TFT_BLACK);
      canvas.fillRect(30, 50 + (i * 25), 180, 18, TFT_BLACK);
      canvas.drawString(options[i], 40, 54 + (i * 25));
    } else {
      canvas.setTextColor(TFT_BLACK);
      canvas.drawString(options[i], 40, 54 + (i * 25));
    }
  }

  drawNavHint("Up/Down  Enter=Select  `=Exit", 30, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawCreateNetwork() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Create");

  if (networkNameInput.length() == 0) {
    drawTextInputBox("Network Name:", networkPasswordInput, false);
  } else {
    drawTextInputBox("Password (8+ chars):", networkPasswordInput, true);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawJoinNetwork() {
  canvas.fillScreen(TFT_WHITE);

  // If coming from Room Radar, show room name
  if (lobbyRoomName.length() > 0) {
    String header = "Join: " + lobbyRoomName;
    drawLabChatHeader(header.c_str());
  } else {
    drawLabChatHeader("Join");
  }

  // Draw input box
  canvas.fillRoundRect(20, 50, 200, 60, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 50, 200, 60, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 51, 198, 58, 11, TFT_BLACK);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Room Key:", 30, 60);

  // Input field with password masking
  String displayText = networkPasswordInput;
  if (networkPasswordInput.length() > 0) {
    displayText = "";
    for (int i = 0; i < networkPasswordInput.length(); i++) {
      displayText += "*";
    }
  }

  if (displayText.length() > 28) {
    displayText = displayText.substring(displayText.length() - 28);
  }

  canvas.drawString(displayText.c_str(), 30, 75);

  // Blinking cursor
  if (cursorVisible) {
    int cursorX = 30 + (displayText.length() * 6);
    canvas.drawLine(cursorX, 75, cursorX, 83, TFT_BLACK);
  }

  // Simple hint - just Esc to Settings
  drawNavHint("Esc=Settings", 85, 95);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawMainChat() {
  canvas.fillScreen(TFT_WHITE);

  // Header with channel name and DM indicator
  char subtitle[64];
  if (dmTargetID.length() > 0) {
    snprintf(subtitle, 64, "DM:%s [%d]", dmTargetUsername.c_str(), espNowManager.getPeerCount());
  } else {
    String roomColor = activeRooms[currentRoomIndex].color;
    snprintf(subtitle, 64, "%s [%d]", roomColor.c_str(), espNowManager.getPeerCount());
  }
  drawLabChatHeader(subtitle);

  // Message area (with margin for input box)
  canvas.fillRoundRect(5, 32, 230, 70, 10, TFT_BLACK);
  canvas.drawRoundRect(5, 32, 230, 70, 10, TFT_ORANGE);

  // Display messages with wrapping (4 messages visible with 2-line support)
  canvas.setTextSize(1);
  int messageCount = messageHandler.getQueueCount();

  // Filter messages by channel or DM mode - build filtered list first
  int filteredIndices[MESSAGE_QUEUE_SIZE];
  int filteredCount = 0;

  for (int i = 0; i < messageCount; i++) {
    DisplayMessage* msg = messageHandler.getQueuedMessage(i);
    if (!msg) continue;

    // Filter: if in DM mode, only show DMs with target, otherwise show channel messages
    bool shouldDisplay = false;
    if (msg->type == MSG_SYSTEM) {
      // Always show system messages
      shouldDisplay = (msg->channel == chatCurrentChannel || dmTargetID.length() == 0);
    } else if (dmTargetID.length() > 0) {
      // DM mode: show only direct messages with this user
      shouldDisplay = (msg->type == MSG_DIRECT);
    } else {
      // Channel mode: show only broadcasts on current channel
      shouldDisplay = (msg->type == MSG_BROADCAST && msg->channel == chatCurrentChannel);
    }

    if (shouldDisplay) {
      filteredIndices[filteredCount++] = i;
    }
  }

  // Display last 4 messages (newest at bottom), accounting for scroll
  int lineY = 40;
  int displayedCount = 0;
  int startIndex = max(0, filteredCount - 4 - scrollPosition);
  int endIndex = min(filteredCount, startIndex + 4);

  for (int idx = startIndex; idx < endIndex; idx++) {
    int i = filteredIndices[idx];
    DisplayMessage* msg = messageHandler.getQueuedMessage(i);
    if (!msg) continue;

    // Emoji chunks are now processed via callback, won't be in queue

    // Get solid user color from sequential peer index (fire colors for black bg)
    uint16_t userColors[] = {
      0xFFE0,  // Yellow
      0xFD20,  // Orange
      0xF800,  // Red
      0x07FF,  // Baby Blue (Cyan)
      0x001F   // Blue
    };

    // Find peer index for this device to assign unique sequential color
    int colorIndex = 0;
    PeerDevice* peer = espNowManager.findPeerByDeviceID(msg->deviceID);
    if (peer) {
      // Find the index of this peer in the peer list
      for (int p = 0; p < espNowManager.getPeerCount(); p++) {
        PeerDevice* checkPeer = espNowManager.getPeer(p);
        if (checkPeer && strcmp(checkPeer->deviceID, msg->deviceID) == 0) {
          colorIndex = p % 5;
          break;
        }
      }
    } else {
      // Fallback to hash if peer not found (shouldn't happen)
      uint32_t deviceHash = 0;
      for (int j = 0; j < strlen(msg->deviceID); j++) {
        deviceHash = deviceHash * 31 + msg->deviceID[j];
      }
      colorIndex = deviceHash % 5;
    }
    uint16_t userColor = userColors[colorIndex];

    // Reverse gradient colors for message content
    uint16_t gradientColors[] = {
      0xFFFF,  // White
      0x07FF,  // Cyan
      0x001F,  // Blue
      0x780F,  // Purple
      0xF800,  // Red
      0xFD20,  // Orange
      0xFFE0,  // Yellow
    };
    int numColors = 7;

    auto getMessageGradientColor = [&](int charIndex) -> uint16_t {
      int cycleLength = 80;
      int posInCycle = charIndex % cycleLength;
      float t;

      if (posInCycle < 40) {
        t = (float)posInCycle / 40.0f;
      } else {
        t = (float)(80 - posInCycle) / 40.0f;
      }

      float colorPosition = t * (numColors - 1);
      int colorIndex1 = (int)colorPosition;
      int colorIndex2 = (colorIndex1 + 1) % numColors;
      float blend = colorPosition - colorIndex1;

      return interpolateColor(gradientColors[colorIndex1], gradientColors[colorIndex2], blend);
    };

    // System messages: display centered with terminal gradient
    if (msg->type == MSG_SYSTEM) {
      canvas.setTextSize(1);
      String systemMsg = "* " + msg->content + " *";
      int textWidth = systemMsg.length() * 6;
      int centerX = 120 - (textWidth / 2);

      // Terminal input gradient - more yellow emphasis
      uint16_t sysGradientColors[] = {0xFFE0, 0xFFE0, 0xFD20, 0xF800, 0x780F, 0x001F, 0x07FF, 0xFFFF};

      for (int c = 0; c < systemMsg.length(); c++) {
        int cycleLength = 80;
        int posInCycle = c % cycleLength;
        float t = (posInCycle < 40) ? ((float)posInCycle / 40.0f) : ((float)(80 - posInCycle) / 40.0f);

        float colorPosition = t * 7.0f;
        int colorIndex1 = (int)colorPosition;
        int colorIndex2 = (colorIndex1 + 1) % 8;
        float blend = colorPosition - colorIndex1;

        uint16_t color = interpolateColor(sysGradientColors[colorIndex1], sysGradientColors[colorIndex2], blend);
        canvas.setTextColor(color);
        canvas.drawChar(systemMsg.charAt(c), centerX + (c * 6), lineY);
      }

      lineY += 10;
      continue;
    }

    String username = String(msg->username) + ": ";
    String content = msg->content;

    // Draw username with solid color, size 1
    canvas.setTextSize(1);
    canvas.setTextColor(userColor);
    int xPos = 10;
    canvas.drawString(username.c_str(), xPos, lineY);
    xPos += username.length() * 6;

    // Peppermint pattern - alternate red/white per message
    uint16_t messageColor = (i % 2 == 0) ? TFT_RED : TFT_WHITE;

    // Draw message content with emoji support at 1x scale
    canvas.setTextSize(1);
    canvas.setTextColor(messageColor);

    // Calculate remaining space (in pixels, not chars, since emojis are slightly larger)
    int remainingPixels = 220 - (xPos - 10); // Message area is ~220px wide

    // Simple rendering - just draw and let it wrap naturally
    // Check if content fits on one line (rough estimate)
    int contentWidth = 0;
    const char* contentStr = content.c_str();
    int contentLen = strlen(contentStr);

    // Estimate width: regular chars = 6px, emojis = 8px
    for (int c = 0; c < contentLen;) {
      bool isEmoji = false;
      if (c + 3 < contentLen) {
        const char* check = &contentStr[c];
        if (memcmp(check, "\xF0\x9F\x8D\x93", 4) == 0 || memcmp(check, "\xF0\x9F\x8D\x8D", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x8D\xB0", 4) == 0 || memcmp(check, "\xF0\x9F\x8D\x89", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x90\x9A", 4) == 0 || memcmp(check, "\xF0\x9F\x8D\xAC", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x94\xA5", 4) == 0 || memcmp(check, "\xF0\x9F\x92\x80", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x9A\x80", 4) == 0 || memcmp(check, "\xF0\x9F\x8E\xB5", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x91\xBE", 4) == 0 || memcmp(check, "\xF0\x9F\x8E\xAE", 4) == 0 ||
            memcmp(check, "\xF0\x9F\x8C\x99", 4) == 0 || memcmp(check, "\xF0\x9F\x92\x8E", 4) == 0) {
          contentWidth += 8;
          c += 4;
          isEmoji = true;
        }
      }
      if (!isEmoji && c + 2 < contentLen) {
        const char* check = &contentStr[c];
        if (memcmp(check, "\xE2\xAD\x90", 3) == 0 || memcmp(check, "\xE2\x98\x95", 3) == 0 ||
            memcmp(check, "\xE2\x9A\xA1", 3) == 0 || memcmp(check, "\xE2\x9D\xA4", 3) == 0) {
          contentWidth += 8;
          c += 3;
          isEmoji = true;
        }
      }
      if (!isEmoji) {
        contentWidth += 6;
        c++;
      }
    }

    if (contentWidth <= remainingPixels) {
      // Single line
      drawTextWithEmojis(content.c_str(), xPos, lineY, messageColor);
      lineY += 10;
    } else {
      // Two lines - split at first space after halfway point
      int splitIdx = contentLen / 2;
      for (int s = splitIdx; s < contentLen && s < splitIdx + 10; s++) {
        if (contentStr[s] == ' ') {
          splitIdx = s;
          break;
        }
      }

      String line1 = content.substring(0, splitIdx);
      String line2 = content.substring(splitIdx);

      drawTextWithEmojis(line1.c_str(), xPos, lineY, messageColor);
      lineY += 8;
      drawTextWithEmojis(line2.c_str(), 10, lineY, messageColor);
      lineY += 10;
    }

    displayedCount++;
  }

  // Show emoji hint with berry centered below if no messages
  if (filteredCount == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_DARKGREY);

    // Draw text centered
    int y = 65;
    canvas.drawString("\\ = emojis", 86, y);

    // Draw berry emoji centered on next line
    const char* berry = "\xF0\x9F\x8D\x93";  // 🍓 Strawberry
    drawEmojiIcon(112, y + 10, berry, TFT_RED, 1);
  }

  // Input area (black background with yellow outline, terminal style, with margins)
  canvas.fillRoundRect(5, 110, 230, 21, 8, TFT_BLACK);
  canvas.drawRoundRect(5, 110, 230, 21, 8, TFT_YELLOW);

  // Terminal gradient colors (smooth ping-pong)
  uint16_t gradientColors[] = {
    0xFFE0,  // Yellow
    0xFD20,  // Orange
    0xF800,  // Red
    0x780F,  // Purple
    0x001F,  // Blue
    0x07FF,  // Cyan
    0xFFFF,  // White
  };
  int numColors = 7;

  // Smooth gradient helper (from terminal)
  auto getInputGradientColor = [&](int charIndex) -> uint16_t {
    int cycleLength = 80;
    int posInCycle = charIndex % cycleLength;
    float t;

    if (posInCycle < 40) {
      t = (float)posInCycle / 40.0f;
    } else {
      t = (float)(80 - posInCycle) / 40.0f;
    }

    float colorPosition = t * (numColors - 1);
    int colorIndex1 = (int)colorPosition;
    int colorIndex2 = (colorIndex1 + 1) % numColors;
    float blend = colorPosition - colorIndex1;

    return interpolateColor(gradientColors[colorIndex1], gradientColors[colorIndex2], blend);
  };

  // Draw prompt "> " and input with smooth gradient
  String fullInput = String("> ") + chatInput;
  String displayInput = fullInput;
  int maxChars = 18;  // Reduced for larger text

  if (displayInput.length() > maxChars) {
    displayInput = displayInput.substring(displayInput.length() - maxChars);
  }

  int xPos = 10;
  int inputY = 115;  // Text Y position (5px from top of input box at y=110)
  for (int i = 0; i < displayInput.length(); i++) {
    int actualIndex = (fullInput.length() > maxChars) ? (fullInput.length() - maxChars + i) : i;
    canvas.setTextSize(2);  // Changed from 1 to 2
    canvas.setTextColor(getInputGradientColor(actualIndex));
    canvas.drawString(String(displayInput[i]).c_str(), xPos, inputY);
    xPos += 12;  // Doubled character spacing
  }

  // Cursor
  if (cursorVisible) {
    int cursorX = 10 + (displayInput.length() * 12);  // Doubled spacing
    if (cursorX < 230) {
      canvas.fillRect(cursorX, inputY, 12, 16, getInputGradientColor(fullInput.length()));  // Doubled cursor size
    }
  }

  // Emoji hint inside message area bottom (only when input is empty)
  if (chatInput.length() == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_DARKGREY);
    int hintY = 90;  // Inside message area (ends at y=102)

    // Draw "\=emojis" first
    canvas.drawString("\\=emojis", 45, hintY);

    // Draw berry centered with text - 45 + 8*8 + 12 = 121
    int berryX = 121;
    int berryY = hintY - 4;  // Vertically center 16px berry with 8px text
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        uint16_t color = BERRY_ICON[y][x];
        if (color != 0x07E0) { // Skip transparent pixels
          canvas.drawPixel(berryX + x, berryY + y, color);
        }
      }
    }

    // Draw "  `=settings" after berry - 121 + 16 + 12 = 149
    canvas.drawString("  `=settings", 149, hintY);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawUserList() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Users");

  // User list box
  canvas.fillRoundRect(20, 35, 200, 75, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 35, 200, 75, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 36, 198, 73, 11, TFT_BLACK);

  int peerCount = espNowManager.getPeerCount();

  if (peerCount == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("No users online", 70, 65);
  } else {
    for (int i = 0; i < min(5, peerCount); i++) {
      PeerDevice* peer = espNowManager.getPeer(i);
      if (!peer) continue;

      canvas.setTextSize(1);
      canvas.setTextColor(TFT_BLACK);

      String userLine = String(peer->username) + " (" + String(peer->deviceID) + ")";
      if (userLine.length() > 30) {
        userLine = userLine.substring(0, 27) + "...";
      }

      canvas.drawString(userLine.c_str(), 30, 42 + (i * 12));
    }

    if (peerCount > 5) {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("...and more", 75, 100);
    }
  }

  drawNavHint("`=Back", 100, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawChatSettings() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Settings");

  // Settings box (taller for 5 items)
  canvas.fillRoundRect(20, 35, 200, 100, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 35, 200, 100, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 36, 198, 98, 11, TFT_BLACK);

  const char* options[] = {"Change Username", "Room: ", "Room Radar", "Manage Emojis", "Leave Network"};

  for (int i = 0; i < 5; i++) {
    canvas.setTextSize(1);
    if (i == chatSettingsMenuIndex) {
      canvas.setTextColor(TFT_BLACK);
      canvas.fillRect(30, 40 + (i * 18), 180, 14, TFT_BLACK);
      if (i == 1) {
        // Room - show current room name or "New Room"
        String roomText = String(options[i]);
        if (currentRoomIndex >= 0 && currentRoomIndex < activeRoomCount) {
          roomText += activeRooms[currentRoomIndex].name;
        } else {
          roomText += "New Room";
        }
        canvas.drawString(roomText.c_str(), 40, 43 + (i * 18));
      } else if (i == 3) {
        // Manage Emojis - show count
        String emojiText = String(options[i]) + " (" + String(systemEmojiCount) + "/20)";
        canvas.drawString(emojiText.c_str(), 40, 43 + (i * 18));
      } else {
        canvas.drawString(options[i], 40, 43 + (i * 18));
      }
    } else {
      canvas.setTextColor(TFT_BLACK);
      if (i == 1) {
        // Room - show current room name or "New Room"
        String roomText = String(options[i]);
        if (currentRoomIndex >= 0 && currentRoomIndex < activeRoomCount) {
          roomText += activeRooms[currentRoomIndex].name;
        } else {
          roomText += "New Room";
        }
        canvas.drawString(roomText.c_str(), 40, 43 + (i * 18));
      } else if (i == 3) {
        // Manage Emojis - show count
        String emojiText = String(options[i]) + " (" + String(systemEmojiCount) + "/20)";
        canvas.drawString(emojiText.c_str(), 40, 43 + (i * 18));
      } else {
        canvas.drawString(options[i], 40, 43 + (i * 18));
      }
    }
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawChannelSwitch() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Switch Room");

  // Room selector box
  canvas.fillRoundRect(30, 45, 180, 50, 10, TFT_WHITE);
  canvas.drawRoundRect(30, 45, 180, 50, 10, TFT_RED);
  canvas.drawRoundRect(31, 46, 178, 48, 9, TFT_RED);

  canvas.setTextSize(2);
  String roomName;
  if (currentRoomIndex < activeRoomCount) {
    roomName = activeRooms[currentRoomIndex].name;
  } else {
    roomName = "New Room";
  }

  canvas.setTextColor(TFT_BLACK);
  int textWidth = roomName.length() * 12;
  int xPos = 120 - (textWidth / 2);
  canvas.drawString(roomName.c_str(), xPos, 60);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("< Left/Right >", 85, 105);

  drawNavHint("`=Back  Enter=Select", 60, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawDMSelect() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Direct Message");

  canvas.fillRoundRect(20, 35, 200, 75, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 35, 200, 75, 12, TFT_BLACK);
  canvas.drawRoundRect(21, 36, 198, 73, 11, TFT_BLACK);

  int peerCount = espNowManager.getPeerCount();

  if (peerCount == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("No users online", 70, 65);
  } else {
    for (int i = 0; i < min(5, peerCount); i++) {
      PeerDevice* peer = espNowManager.getPeer(i);
      if (!peer) continue;

      canvas.setTextSize(1);

      if (i == selectedUserIndex) {
        canvas.setTextColor(TFT_BLACK);
        canvas.fillRect(30, 42 + (i * 12), 180, 10, TFT_BLACK);
      } else {
        canvas.setTextColor(TFT_BLACK);
      }

      String userLine = String(peer->username);
      if (userLine.length() > 28) {
        userLine = userLine.substring(0, 25) + "...";
      }
      canvas.drawString(userLine.c_str(), 35, 42 + (i * 12));
    }
  }

  drawNavHint("Up/Down  Enter=Select  `=Back", 30, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawNetworkInfo() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Network Info");

  canvas.fillRoundRect(20, 35, 200, 75, 12, TFT_WHITE);
  canvas.drawRoundRect(20, 35, 200, 75, 12, TFT_BLACK);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);

  String networkName = securityManager.getNetworkName();
  String deviceID = securityManager.getDeviceID();
  int peerCount = espNowManager.getPeerCount();

  canvas.drawString(("Network: " + networkName).c_str(), 30, 45);
  canvas.drawString(("Device: " + deviceID).c_str(), 30, 60);
  canvas.drawString(("Peers: " + String(peerCount)).c_str(), 30, 75);

  drawNavHint("`=Back", 100, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawChangeUsername() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Username");

  drawTextInputBox("New Username:", usernameInput, false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Max 15 characters", 70, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawRenameChannel() {
  canvas.fillScreen(TFT_WHITE);
  char subtitle[32];
  snprintf(subtitle, 32, "Rename #%s", channelNames[chatCurrentChannel].c_str());
  drawLabChatHeader(subtitle);

  drawTextInputBox("Channel Name:", channelNameInput, false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Max 15 characters", 70, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawEmojiPicker() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Emojis");

  // Picker box (single unified area)
  canvas.fillRoundRect(10, 35, 220, 75, 12, TFT_BLACK);
  canvas.drawRoundRect(10, 35, 220, 75, 12, TFT_ORANGE);

  // Info text
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_ORANGE);
  String slotInfo = String(systemEmojiCount) + "/20 loaded";
  canvas.drawString(slotInfo.c_str(), 85, 40);

  // Draw emojis in two rows (12 per row, max 20 visible)
  int startX = 20;
  int startY = 55;
  int spacingX = 18;
  int spacingY = 25;
  int emojisPerRow = 12;

  Serial.print("Drawing ");
  Serial.print(systemEmojiCount);
  Serial.println(" emojis");

  // Draw up to 20 emojis (12 in first row, 8 in second)
  for (int i = 0; i < systemEmojiCount && i < 20; i++) {
    int row = i / emojisPerRow;
    int col = i % emojisPerRow;
    int x = startX + (col * spacingX);
    int y = startY + (row * spacingY);

    // Highlight selected emoji
    if (i == selectedEmojiIndex) {
      canvas.fillRoundRect(x - 2, y - 2, 18, 18, 4, TFT_ORANGE);
    }

    // Draw custom emoji at 1x scale (16x16)
    drawCustomEmoji(x, y, i, 1);
  }

  // Nav hints
  drawNavHint("Arrows=Nav Enter=Add `=Back", 50, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawEmojiManager() {
  canvas.fillScreen(TFT_WHITE);
  drawLabChatHeader("Manage Emojis");

  // Info text
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  String slotInfo = "Slots: " + String(systemEmojiCount) + "/20";
  canvas.drawString(slotInfo.c_str(), 85, 30);

  // Draw emoji grid (4 columns x 5 rows = 20 slots)
  int startX = 20;
  int startY = 45;
  int spacingX = 52;
  int spacingY = 18;

  for (int i = 0; i < 20; i++) {
    int row = i / 4;
    int col = i % 4;
    int x = startX + (col * spacingX);
    int y = startY + (row * spacingY);

    // Slot box
    if (i == selectedEmojiManagerIndex) {
      canvas.drawRect(x - 2, y - 2, 50, 16, TFT_YELLOW);
      canvas.drawRect(x - 1, y - 1, 48, 14, TFT_YELLOW);
    } else {
      canvas.drawRect(x - 1, y - 1, 48, 14, TFT_DARKGREY);
    }

    if (i < systemEmojiCount) {
      // Draw emoji at 1x scale
      drawCustomEmoji(x, y, i, 1);

      // Draw shortcut name (truncated)
      String shortcut = systemEmojis[i].shortcut;
      if (shortcut.length() > 5) shortcut = shortcut.substring(0, 5);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString(shortcut.c_str(), x + 18, y + 2);
    } else {
      // Empty slot
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_LIGHTGREY);
      canvas.drawString("empty", x + 10, y + 4);
    }
  }

  // Nav hints
  if (selectedEmojiManagerIndex < systemEmojiCount && systemEmojis[selectedEmojiManagerIndex].shortcut != "strawberry") {
    drawNavHint("Arrows=Nav  Del=Remove  `=Back", 35, 118);
  } else {
    drawNavHint("Arrows=Nav  `=Back", 70, 118);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawLobby() {
  canvas.fillScreen(TFT_WHITE);

  String header = "Lobby: " + lobbyRoomName;
  drawLabChatHeader(header.c_str());

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Knocking...", 70, 50);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Waiting for room access", 60, 75);

  // Show animated dots for waiting
  unsigned long now = millis();
  int dotCount = ((now / 500) % 4);
  String dots = "";
  for (int i = 0; i < dotCount; i++) {
    dots += ".";
  }
  canvas.drawString(dots.c_str(), 175, 75);

  drawNavHint("Esc=Cancel", 90, 118);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawRoomRadar() {
  canvas.fillScreen(TFT_WHITE);

  if (radarTrackingMode) {
    // TRACKING MODE - Hot/Cold display
    drawLabChatHeader("Tracking");

    if (selectedRadarIndex < nearbyDeviceCount) {
      NearbyDevice& device = nearbyDevices[selectedRadarIndex];

      // Show room/device name with device ID suffix
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_BLACK);
      String displayName = device.roomName;
      if (device.deviceID.length() >= 2) {
        displayName += " (" + device.deviceID.substring(device.deviceID.length() - 2) + ")";
      }
      int textWidth = displayName.length() * 12;
      int xPos = 120 - (textWidth / 2);
      canvas.drawString(displayName.c_str(), xPos, 35);

      // Calculate proximity (rssi ranges: -30 = very close, -90 = far)
      int strength = constrain(map(device.rssi, -90, -30, 0, 10), 0, 10);

      // Draw strength indicator with color
      canvas.setTextSize(3);
      uint16_t heatColor;
      String heatText;
      if (strength >= 8) {
        heatColor = TFT_RED;
        heatText = "HOT";
      } else if (strength >= 5) {
        heatColor = TFT_ORANGE;
        heatText = "WARM";
      } else {
        heatColor = TFT_BLUE;
        heatText = "COLD";
      }
      canvas.setTextColor(heatColor);
      int heatX = 120 - (heatText.length() * 18 / 2);
      canvas.drawString(heatText.c_str(), heatX, 60);

      // Draw signal bar
      canvas.fillRect(30, 95, 180, 10, TFT_LIGHTGREY);
      canvas.fillRect(30, 95, strength * 18, 10, heatColor);
    } else {
      // No device to track yet
      canvas.setTextSize(2);
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Searching...", 65, 60);
    }

    canvas.setTextSize(1);
    drawNavHint("`=Back  T=List", 70, 118);
  } else {
    // LIST MODE - Show nearby rooms/devices
    drawLabChatHeader("Room Radar");

    canvas.setTextSize(1);

    if (nearbyDeviceCount == 0) {
      canvas.setTextColor(TFT_RED);
      canvas.drawString("No nearby rooms found", 55, 45);
      canvas.setTextColor(TFT_BLUE);
      canvas.drawString("Scanning for beacons...", 50, 60);
      canvas.drawString("Devices broadcast every 10s", 35, 70);
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Press esc and then come back,", 25, 85);
      canvas.drawString("needs to reload sometimes.", 35, 95);
    } else {
      // Draw list of nearby devices/rooms
      int yPos = 40;
      for (int i = 0; i < nearbyDeviceCount && i < 5; i++) {
        bool isSelected = (i == selectedRadarIndex);
        NearbyDevice& device = nearbyDevices[i];

        if (isSelected) {
          canvas.fillRoundRect(10, yPos - 2, 220, 14, 3, TFT_LIGHTGREY);
        }

        // Show room name with device ID suffix
        String displayName = device.roomName;
        if (device.deviceID.length() >= 2) {
          displayName += " (" + device.deviceID.substring(device.deviceID.length() - 2) + ")";
        }
        canvas.setTextColor(TFT_BLACK);
        canvas.drawString(displayName.c_str(), 15, yPos);

        // Draw signal strength bars
        int bars = constrain(map(device.rssi, -90, -30, 1, 5), 1, 5);
        int barX = 190;
        for (int b = 0; b < 5; b++) {
          if (b < bars) {
            canvas.fillRect(barX + (b * 6), yPos + 10 - (b * 2), 4, 2 + (b * 2), TFT_GREEN);
          } else {
            canvas.drawRect(barX + (b * 6), yPos + 10 - (b * 2), 4, 2 + (b * 2), TFT_LIGHTGREY);
          }
        }

        yPos += 16;
      }
    }

    drawNavHint("Up/Down  Enter=Knock  T=Track  `=Back", 15, 118);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void enterRoomRadar() {
  chatActive = true;  // Enable navigation
  selectedRadarIndex = 0;
  radarTrackingMode = false;
  nearbyDeviceCount = 0;
  lastRadarScan = 0;

  // Populate nearby devices from ESP-NOW peer list
  updateRadarDeviceList();
}

void exitRoomRadar() {
  radarTrackingMode = false;
  nearbyDeviceCount = 0;
}

void updateRadarDeviceList() {
  // Clear current list
  nearbyDeviceCount = 0;

  // Get room name from security manager (if in a network)
  const char* currentRoomName = securityManager.isConnected() ?
    securityManager.getNetworkName() : "Unknown Room";

  // Scan ESP-NOW peers
  int peerCount = espNowManager.getPeerCount();
  for (int i = 0; i < peerCount && nearbyDeviceCount < MAX_NEARBY_DEVICES; i++) {
    PeerDevice* peer = espNowManager.getPeer(i);
    if (peer && peer->active) {
      // Calculate time since last seen
      unsigned long timeSince = millis() - peer->lastSeen;

      // Only show peers seen in last 60 seconds
      if (timeSince < 60000) {
        nearbyDevices[nearbyDeviceCount].deviceID = String(peer->deviceID);

        // Show room name if available (from beacon), otherwise username, otherwise device ID
        if (strlen(peer->roomName) > 0 && strcmp(peer->roomName, "No Room") != 0) {
          nearbyDevices[nearbyDeviceCount].roomName = String(peer->roomName);
        } else if (strlen(peer->username) > 0) {
          nearbyDevices[nearbyDeviceCount].roomName = String(peer->username) + " (no room)";
        } else {
          nearbyDevices[nearbyDeviceCount].roomName = String(peer->deviceID);
        }

        // Use smoothed RSSI with gentle degradation
        // Fresh (0s) = use smoothed value, degrade slowly over time
        int timeSeconds = timeSince / 1000;
        int degradedRSSI = peer->rssiSmoothed - (timeSeconds / 2);  // -0.5 dB per second (gentler)
        degradedRSSI = constrain(degradedRSSI, -90, -30);

        nearbyDevices[nearbyDeviceCount].rssi = degradedRSSI;
        nearbyDevices[nearbyDeviceCount].lastSeen = peer->lastSeen;
        nearbyDeviceCount++;
      }
    }
  }

  lastRadarScan = millis();
}

// ============================================================================
// SYSTEM EMOJI FUNCTIONS
// ============================================================================

// Helper function to draw custom emoji at specified position and scale
void drawCustomEmoji(int screenX, int screenY, int emojiIndex, int scale) {
  if (emojiIndex < 0 || emojiIndex >= systemEmojiCount) return;

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      uint16_t color = systemEmojis[emojiIndex].pixels[y][x];

      // Skip lime green pixels (treat as transparent)
      if (color == TRANSPARENCY_COLOR) continue;

      if (scale == 1) {
        canvas.drawPixel(screenX + x, screenY + y, color);
      } else {
        // Draw scaled pixel
        canvas.fillRect(screenX + (x * scale), screenY + (y * scale), scale, scale, color);
      }
    }
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void loadFriendEmojis() {
  friendEmojiCount = 0;

  Serial.println("================================================");
  Serial.println("=== loadFriendEmojis() called ===");

  // Load friend emojis from SD card if available
  if (!SD.exists("/labchat/received_emojis")) {
    Serial.println("❌ Directory /labchat/received_emojis does NOT exist");
    Serial.println("================================================");
    return; // No received emojis yet
  }

  Serial.println("✓ Directory /labchat/received_emojis exists");

  File dir = SD.open("/labchat/received_emojis");
  if (!dir) {
    Serial.println("❌ Could not open directory");
    Serial.println("================================================");
    return;
  }
  if (!dir.isDirectory()) {
    Serial.println("❌ Path is not a directory");
    dir.close();
    Serial.println("================================================");
    return;
  }

  Serial.println("✓ Directory opened successfully");

  // Load each .emoji file (up to 12)
  Serial.println("Starting file iteration...");
  File file = dir.openNextFile();
  int fileIndex = 0;
  while (file && friendEmojiCount < 12) {
    String filename = String(file.name());
    fileIndex++;

    Serial.print("File #");
    Serial.print(fileIndex);
    Serial.print(": ");
    Serial.print(filename);
    Serial.print(" (isDir: ");
    Serial.print(file.isDirectory() ? "yes" : "no");
    Serial.println(")");

    if (!file.isDirectory() && filename.endsWith(".emoji")) {
      // Extract just the filename without path (in case it has one)
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash >= 0) {
        filename = filename.substring(lastSlash + 1);
      }

      Serial.print("  → Filename: ");
      Serial.println(filename);

      // Extract shortcut name (remove .emoji extension)
      String shortcut = filename;
      shortcut.replace(".emoji", "");

      Serial.print("  → Shortcut: ");
      Serial.println(shortcut);

      // Construct full path manually
      String fullPath = "/labchat/received_emojis/" + filename;
      Serial.print("  → Full path: ");
      Serial.println(fullPath);

      // Close directory file handle and reopen with full path
      file.close();
      File emojiFile = SD.open(fullPath, FILE_READ);
      if (emojiFile) {
        // Load pixel data
        size_t bytesRead = emojiFile.read((uint8_t*)friendEmojis[friendEmojiCount].pixels, 512);
        emojiFile.close();

        Serial.print("  → Read ");
        Serial.print(bytesRead);
        Serial.println(" bytes");

        friendEmojis[friendEmojiCount].shortcut = shortcut;
        friendEmojiCount++;

        Serial.print("  ✓ Loaded friend emoji: ");
        Serial.print(shortcut);
        Serial.print(" from ");
        Serial.println(fullPath);
      } else {
        Serial.print("  ❌ Failed to open: ");
        Serial.println(fullPath);
      }
    } else {
      Serial.println("  → Skipped (directory or not .emoji)");
      file.close();
    }
    file = dir.openNextFile();
  }
  dir.close();

  Serial.print("✓ Total friend emojis loaded: ");
  Serial.println(friendEmojiCount);
  Serial.println("================================================");
}

void saveFriendEmoji(const String& shortcut, const uint16_t pixels[16][16]) {
  Serial.println("=== saveFriendEmoji called for: " + shortcut);

  // Check if already exists in RAM
  for (int i = 0; i < systemEmojiCount; i++) {
    if (systemEmojis[i].shortcut == shortcut) {
      // Update existing
      memcpy(systemEmojis[i].pixels, pixels, 512);
      Serial.println("Updated existing emoji in RAM: " + shortcut);
      return;
    }
  }

  // Add new to RAM (up to 20 slots)
  if (systemEmojiCount < 20) {
    systemEmojis[systemEmojiCount].shortcut = shortcut;
    memcpy(systemEmojis[systemEmojiCount].pixels, pixels, 512);
    systemEmojiCount++;
    Serial.print("Stored emoji in RAM: " + shortcut);
    Serial.print(" (total: ");
    Serial.print(systemEmojiCount);
    Serial.println(")");
  } else {
    Serial.println("WARNING: Emoji slots full!");
    return;
  }

  // Try to save to SD card if available
  if (SD.begin()) {
    if (!SD.exists("/labchat")) {
      SD.mkdir("/labchat");
    }
    if (!SD.exists("/labchat/emojis")) {
      SD.mkdir("/labchat/emojis");
    }

    String filename = "/labchat/emojis/" + shortcut + ".emoji";
    File file = SD.open(filename, FILE_WRITE);
    if (file) {
      file.write((uint8_t*)pixels, 512);
      file.close();
      Serial.println("Saved to SD: " + filename);
    } else {
      Serial.println("Could not save to SD (no card?)");
    }
  } else {
    Serial.println("SD not available, RAM-only storage");
  }
}

void reloadSystemEmojis() {
  Serial.println("=== reloadSystemEmojis() called ===");
  systemEmojisLoaded = false; // Force reload
  loadSystemEmojis();
  Serial.print("=== Reload complete. Total emojis: ");
  Serial.println(systemEmojiCount);
}

void loadSystemEmojis() {
  if (systemEmojisLoaded) return; // Already loaded this session

  systemEmojiCount = 0;

  // Always add hardcoded star and berry emojis first (work without SD card)
  systemEmojis[systemEmojiCount].shortcut = "star";
  memcpy(systemEmojis[systemEmojiCount].pixels, STAR_ICON, sizeof(STAR_ICON));
  systemEmojiCount++;

  systemEmojis[systemEmojiCount].shortcut = "berry";
  memcpy(systemEmojis[systemEmojiCount].pixels, BERRY_ICON, sizeof(BERRY_ICON));
  systemEmojiCount++;

  Serial.println("=== loadSystemEmojis() - Loading from /labchat/emojis ===");

  // ONE-TIME MIGRATION: Move old received_emojis to new unified folder
  if (SD.exists("/labchat/received_emojis")) {
    Serial.println("Found old /labchat/received_emojis - migrating...");

    // Create new folder if needed
    if (!SD.exists("/labchat/emojis")) {
      SD.mkdir("/labchat/emojis");
    }

    File oldDir = SD.open("/labchat/received_emojis");
    if (oldDir && oldDir.isDirectory()) {
      File file = oldDir.openNextFile();
      int moved = 0;
      while (file) {
        String filename = String(file.name());

        if (!file.isDirectory() && filename.endsWith(".emoji")) {
          // Extract filename
          int lastSlash = filename.lastIndexOf('/');
          if (lastSlash >= 0) {
            filename = filename.substring(lastSlash + 1);
          }

          String oldPath = "/labchat/received_emojis/" + filename;
          String newPath = "/labchat/emojis/" + filename;

          // Read from old location
          file.close();
          File oldFile = SD.open(oldPath, FILE_READ);
          if (oldFile) {
            uint8_t buffer[512];
            size_t bytesRead = oldFile.read(buffer, 512);
            oldFile.close();

            // Write to new location
            File newFile = SD.open(newPath, FILE_WRITE);
            if (newFile) {
              newFile.write(buffer, bytesRead);
              newFile.close();

              // Delete old file
              SD.remove(oldPath.c_str());
              moved++;
              Serial.println("  Migrated: " + filename);
            }
          }
        } else {
          file.close();
        }
        file = oldDir.openNextFile();
      }
      oldDir.close();

      // Remove old directory if empty
      SD.rmdir("/labchat/received_emojis");
      Serial.print("Migration complete! Moved ");
      Serial.print(moved);
      Serial.println(" emojis");
    }
  }

  // Load all emojis from unified folder
  if (!SD.exists("/labchat/emojis")) {
    Serial.println("No /labchat/emojis folder found");
    systemEmojisLoaded = true;
    return;
  }

  File dir = SD.open("/labchat/emojis");
  if (!dir || !dir.isDirectory()) {
    Serial.println("Could not open /labchat/emojis");
    systemEmojisLoaded = true;
    return;
  }

  // Load each .emoji file (up to 20)
  File file = dir.openNextFile();
  while (file && systemEmojiCount < 20) {
    String filename = String(file.name());

    if (!file.isDirectory() && filename.endsWith(".emoji")) {
      // Extract just filename without path
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash >= 0) {
        filename = filename.substring(lastSlash + 1);
      }

      // Extract shortcut name
      String shortcut = filename;
      shortcut.replace(".emoji", "");

      // Construct full path and reopen
      String fullPath = "/labchat/emojis/" + filename;
      file.close();

      File emojiFile = SD.open(fullPath, FILE_READ);
      if (emojiFile) {
        // Load pixel data
        emojiFile.read((uint8_t*)systemEmojis[systemEmojiCount].pixels, 512);
        emojiFile.close();

        systemEmojis[systemEmojiCount].shortcut = shortcut;
        systemEmojiCount++;

        Serial.println("Loaded emoji: " + shortcut);
      }
    } else {
      file.close();
    }
    file = dir.openNextFile();
  }
  dir.close();

  Serial.print("Total emojis loaded: ");
  Serial.println(systemEmojiCount);

  systemEmojisLoaded = true;
}

// ============================================================================
// MAIN FUNCTIONS
// ============================================================================

// Callback for real-time message display updates
// CRITICAL: This runs in ESP-NOW interrupt context - NO blocking operations!
void onMessageReceived() {
  if (chatActive && chatState == CHAT_MAIN) {
    scrollPosition = 0; // Auto-scroll to latest message
    needsRedraw = true; // Set flag for main loop to handle
  } else {
    // Not in chat - set flags for main loop to handle
    hasUnreadMessages = true;
    needsBeep = true;
  }
}

void enterLabChat() {
  chatActive = true;
  hasUnreadMessages = false; // Clear notification when entering chat

  // Register callback for real-time display updates
  messageHandler.setMessageCallback(onMessageReceived);

  // Register callback for emoji chunk processing
  messageHandler.setEmojiChunkCallback(processEmojiChunk);

  // Initialize ESP-NOW early to receive discovery beacons
  // This allows Room Radar to work before joining any network
  if (!espNowManager.isInitialized()) {
    // Use temporary PMK for initialization (will be replaced when joining network)
    uint8_t tempPMK[16] = {0};
    espNowManager.init(tempPMK);
    Serial.println("ESP-NOW initialized for discovery (temp PMK)");
  }

  // Check if device PIN is set
  if (!DevicePIN::isSet()) {
    chatState = CHAT_SETUP_PIN;
    pinInput = "";
  } else {
    chatState = CHAT_ENTER_PIN;
    pinInput = "";
  }

  needsRedraw = true;  // Use flag instead of direct draw
}

void exitLabChat() {
  chatActive = false;
  // Keep ESP-NOW running in background to receive messages
  // Only deinit when explicitly leaving network in settings

  // Reset activity timer to prevent immediate screensaver trigger
  extern unsigned long lastActivityTime;
  lastActivityTime = millis();

  // Clear screen with color matching UI inversion state
  extern bool uiInverted;
  canvas.fillScreen(uiInverted ? TFT_BLACK : TFT_WHITE);
}

void updateLabChat() {
  // Handle beep notification FIRST (works even when chat inactive)
  // CRITICAL: Skip beep during audio playback to prevent I2S conflicts and buffer underruns
  if (needsBeep && !isAudioPlaying() && !isRadioPlaying()) {
    needsBeep = false;
    // Descending then ascending chirp - more interesting
    M5Cardputer.Speaker.tone(1400, 50);
    delay(60);
    M5Cardputer.Speaker.tone(1000, 50);
    delay(60);
    M5Cardputer.Speaker.tone(1600, 90);
  }

  if (!chatActive) return;

  // Handle redraw flag (only when chat active)
  if (needsRedraw) {
    needsRedraw = false;
    needsRedraw = true;  // Use flag instead of direct draw
  }

  // Cursor blink (don't redraw entire screen, just toggle cursor state)
  if (millis() - lastCursorBlink > 500) {
    cursorVisible = !cursorVisible;
    lastCursorBlink = millis();
    // Don't call drawLabChat() here - causes constant flickering
    // The cursor is drawn as part of the normal draw cycle when input changes
  }

  // Broadcast discovery beacon every 10 seconds (unencrypted for Room Radar)
  // AND presence every 30 seconds (encrypted for room members)
  if (chatState == CHAT_MAIN) {
    static unsigned long lastBeacon = 0;
    if (millis() - lastBeacon > 10000) {
      messageHandler.sendBeacon();
      lastBeacon = millis();
    }

    if (millis() - lastPresenceBroadcast > 30000) {
      messageHandler.sendPresence();
      lastPresenceBroadcast = millis();
    }
  }

  // Clean up inactive peers every minute
  static unsigned long lastCleanup = 0;
  if (chatState == CHAT_MAIN && millis() - lastCleanup > 60000) {
    espNowManager.cleanupInactivePeers();
    lastCleanup = millis();
  }

  // LOBBY timeout - show "no answer" after 20 seconds
  if (chatState == CHAT_LOBBY && lobbyKnockTime > 0 && millis() - lobbyKnockTime > 20000) {
    // Timeout - show rejection screen
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextSize(3);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("No Answer", 50, 55);
    delay(2000);

    // Return to settings
    lobbyRoomName = "";
    lobbyKnockTime = 0;
    chatState = CHAT_SETTINGS;
    needsRedraw = true;
  }

  // Update Room Radar device list
  // Faster updates in tracking mode for better responsiveness
  unsigned long radarUpdateInterval = radarTrackingMode ? 500 : 3000;  // 0.5s tracking, 3s list
  if (chatState == CHAT_ROOM_RADAR && millis() - lastRadarScan > radarUpdateInterval) {
    updateRadarDeviceList();
    needsRedraw = true;
  }
}

void drawLabChat() {
  switch (chatState) {
    case CHAT_SETUP_PIN:
      drawPinSetup();
      break;
    case CHAT_ENTER_PIN:
      drawPinEntry();
      break;
    case CHAT_NETWORK_MENU:
      drawNetworkMenu();
      break;
    case CHAT_CREATE_NETWORK:
      drawCreateNetwork();
      break;
    case CHAT_JOIN_NETWORK:
      drawJoinNetwork();
      break;
    case CHAT_MAIN:
      drawMainChat();
      break;
    case CHAT_USER_LIST:
      drawUserList();
      break;
    case CHAT_SETTINGS:
      drawChatSettings();
      break;
    case CHAT_CHANNEL_SWITCH:
      drawChannelSwitch();
      break;
    case CHAT_DM_SELECT:
      drawDMSelect();
      break;
    case CHAT_NETWORK_INFO:
      drawNetworkInfo();
      break;
    case CHAT_CHANGE_USERNAME:
      drawChangeUsername();
      break;
    case CHAT_RENAME_CHANNEL:
      drawRenameChannel();
      break;
    case CHAT_EMOJI_PICKER:
      drawEmojiPicker();
      break;
    case CHAT_EMOJI_MANAGER:
      drawEmojiManager();
      break;
    case CHAT_ROOM_RADAR:
      drawRoomRadar();
      break;
    case CHAT_LOBBY:
      drawLobby();
      break;
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handleLabChatNavigation(char key) {
  // Handle based on current state
  switch (chatState) {
    case CHAT_SETUP_PIN:
    case CHAT_ENTER_PIN: {
      if (key == '\n') { // Enter
        if (pinInput.length() == 4) {
          if (chatState == CHAT_SETUP_PIN) {
            DevicePIN::create(pinInput);
            loadSystemEmojis(); // Load system emojis once per session

            // Auto-create starter room with random password
            String randomPassword = "";
            const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            for (int i = 0; i < 12; i++) {
              randomPassword += chars[random(0, 62)];
            }

            // Join network (auto-generates color-based room name)
            if (securityManager.joinNetwork(randomPassword)) {
              String newNetwork = String(securityManager.getNetworkName());

              // Store in activeRooms
              if (activeRoomCount < MAX_ROOMS) {
                activeRooms[activeRoomCount].name = newNetwork;
                activeRooms[activeRoomCount].password = randomPassword;
                activeRooms[activeRoomCount].color = getColorFromNetworkName(newNetwork);
                currentRoomIndex = activeRoomCount;
                activeRoomCount++;
              }

              // Initialize ESP-NOW with proper PMK
              espNowManager.deinit();
              espNowManager.init(securityManager.getPMK());

              // Go to chat
              chatState = CHAT_MAIN;
              messageHandler.sendPresence();
              lastPresenceBroadcast = millis();
              lastNetworkName = newNetwork;

              // Show welcome message
              String welcomeMsg = "Welcome to " + newNetwork + "!";
              messageHandler.addSystemMessage(welcomeMsg.c_str());
            }

            pinInput = "";
          } else {
            if (DevicePIN::verify(pinInput)) {
              loadSystemEmojis(); // Load all emojis once per session

              // Auto-create starter room with random password
              String randomPassword = "";
              const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
              for (int i = 0; i < 12; i++) {
                randomPassword += chars[random(0, 62)];
              }

              // Join network (auto-generates color-based room name)
              if (securityManager.joinNetwork(randomPassword)) {
                String newNetwork = String(securityManager.getNetworkName());

                // Store in activeRooms
                if (activeRoomCount < MAX_ROOMS) {
                  activeRooms[activeRoomCount].name = newNetwork;
                  activeRooms[activeRoomCount].password = randomPassword;
                  activeRooms[activeRoomCount].color = getColorFromNetworkName(newNetwork);
                  currentRoomIndex = activeRoomCount;
                  activeRoomCount++;
                }

                // Initialize ESP-NOW with proper PMK
                espNowManager.deinit();
                espNowManager.init(securityManager.getPMK());

                // Go to chat
                chatState = CHAT_MAIN;
                messageHandler.sendPresence();
                lastPresenceBroadcast = millis();
                lastNetworkName = newNetwork;

                // Show welcome message
                String welcomeMsg = "Welcome to " + newNetwork + "!";
                messageHandler.addSystemMessage(welcomeMsg.c_str());
              }

              pinInput = "";
            } else {
              pinInput = "";
              needsRedraw = true;  // Use flag instead of direct draw
            }
          }
        }
      } else if (key == 8 || key == 127) { // Backspace/Del
        if (pinInput.length() > 0) {
          pinInput.remove(pinInput.length() - 1);
        }
      } else if (key == '`') {
        exitLabChat();
        currentScreenNumber = 0;
        currentState = MAIN_MENU;
        extern void drawScreen(bool inverted);
        extern bool uiInverted;
        drawScreen(uiInverted);
        return;
      } else if (pinInput.length() < 4) {
        // Validate character
        if ((key >= '0' && key <= '9') ||
            (key >= 'A' && key <= 'Z') ||
            (key >= 'a' && key <= 'z') ||
            key == '!' || key == '@' || key == '#' || key == '$' || key == '%' ||
            key == '^' || key == '&' || key == '*' || key == '(' || key == ')') {
          pinInput += key;
        }
      }
      break;
    }

    case CHAT_NETWORK_MENU: {
      if (key == ';') { // Up
        networkMenuIndex = (networkMenuIndex - 1 + 2) % 2;
      } else if (key == '.') { // Down
        networkMenuIndex = (networkMenuIndex + 1) % 2;
      } else if (key == '\n') { // Enter
        if (networkMenuIndex == 0) {
          chatState = CHAT_CREATE_NETWORK;
          networkPasswordInput = "";
          networkNameInput = "";
        } else {
          chatState = CHAT_JOIN_NETWORK;
          networkPasswordInput = "";
        }
      } else if (key == '`') {
        exitLabChat();
        extern void drawScreen(bool inverted);
        extern bool uiInverted;
        currentScreenNumber = 0;
        currentState = MAIN_MENU;
        drawScreen(uiInverted);
        return;
      }
      break;
    }

    case CHAT_CREATE_NETWORK:
    case CHAT_JOIN_NETWORK: {
      if (key == '\n') { // Enter
        if (chatState == CHAT_CREATE_NETWORK) {
          if (networkNameInput.length() == 0) {
            // First step: get name
            networkNameInput = networkPasswordInput;
            networkPasswordInput = "";
          } else if (networkPasswordInput.length() >= 8) {
            // Second step: create network
            String oldNetwork = String(securityManager.getNetworkName());
            if (securityManager.createNetwork(networkPasswordInput, networkNameInput)) {
              String newNetwork = String(securityManager.getNetworkName());

              // Only clear messages if switching to a different network
              if (oldNetwork != newNetwork && oldNetwork.length() > 0) {
                messageHandler.clearQueue();
              }

              // Store this room in activeRooms
              bool roomExists = false;
              for (int i = 0; i < activeRoomCount; i++) {
                if (activeRooms[i].password == networkPasswordInput) {
                  roomExists = true;
                  currentRoomIndex = i;
                  break;
                }
              }
              if (!roomExists && activeRoomCount < MAX_ROOMS) {
                activeRooms[activeRoomCount].name = newNetwork;
                activeRooms[activeRoomCount].password = networkPasswordInput;
                activeRooms[activeRoomCount].color = getColorFromNetworkName(newNetwork);
                currentRoomIndex = activeRoomCount;
                activeRoomCount++;
              }

              espNowManager.deinit();
              espNowManager.init(securityManager.getPMK());
              chatState = CHAT_MAIN;
              messageHandler.sendPresence();
              lastPresenceBroadcast = millis();
              lastNetworkName = newNetwork;
            }
          }
        } else { // JOIN
          if (networkPasswordInput.length() >= 8) {
            String oldNetwork = String(securityManager.getNetworkName());
            if (securityManager.joinNetwork(networkPasswordInput)) {
              String newNetwork = String(securityManager.getNetworkName());

              // Only clear messages if switching to a different network
              if (oldNetwork != newNetwork && oldNetwork.length() > 0) {
                messageHandler.clearQueue();
              }

              // Store this room in activeRooms (if not already there)
              bool roomExists = false;
              for (int i = 0; i < activeRoomCount; i++) {
                if (activeRooms[i].password == networkPasswordInput) {
                  roomExists = true;
                  currentRoomIndex = i;
                  break;
                }
              }
              if (!roomExists && activeRoomCount < MAX_ROOMS) {
                activeRooms[activeRoomCount].name = newNetwork;
                activeRooms[activeRoomCount].password = networkPasswordInput;
                activeRooms[activeRoomCount].color = getColorFromNetworkName(newNetwork);
                currentRoomIndex = activeRoomCount;
                activeRoomCount++;
              }

              espNowManager.deinit();
              espNowManager.init(securityManager.getPMK());
              chatState = CHAT_MAIN;
              messageHandler.sendPresence();
              lastPresenceBroadcast = millis();
              lastNetworkName = newNetwork;
            }
          }
        }
      } else if (key == 8 || key == 127) {
        if (networkPasswordInput.length() > 0) {
          networkPasswordInput.remove(networkPasswordInput.length() - 1);
        }
      } else if (key == '`') {
        // Esc from room key entry - go to settings
        if (chatState == CHAT_JOIN_NETWORK) {
          chatState = CHAT_SETTINGS;
        } else {
          chatState = CHAT_NETWORK_MENU;
        }
        networkPasswordInput = "";
        networkNameInput = "";
      } else if (networkPasswordInput.length() < 50) {
        networkPasswordInput += key;
      }
      break;
    }

    case CHAT_MAIN: {
      // Text input mode - handle typing first
      if (key == '\n') { // Enter - send message
        if (chatInput.length() > 0) {
          // FIRST: Send emoji chunks BEFORE text message so receiver has emojis ready!
          // Parse for :shortcut: patterns
          Serial.println("==================================================");
          Serial.print("📤 SENDING MESSAGE: ");
          Serial.println(chatInput);
          Serial.print("Checking for custom emojis in message. systemEmojiCount=");
          Serial.println(systemEmojiCount);

          int i = 0;
          while (i < chatInput.length()) {
            if (chatInput[i] == ':') {
              int endColon = chatInput.indexOf(':', i + 1);
              if (endColon > i + 1) {
                String shortcut = chatInput.substring(i + 1, endColon);
                Serial.print("Found :shortcut: = ");
                Serial.println(shortcut);

                // Check if this is a system OR friend emoji
                bool foundEmoji = false;
                uint8_t* pixelBytes = nullptr;

                // First check system emojis
                for (int e = 0; e < systemEmojiCount; e++) {
                  if (systemEmojis[e].shortcut == shortcut) {
                    foundEmoji = true;
                    pixelBytes = (uint8_t*)systemEmojis[e].pixels;
                    Serial.println("  MATCH in systemEmojis! Preparing to send...");
                    break;
                  }
                }

                // If not found in system, check friend emojis
                if (!foundEmoji) {
                  for (int e = 0; e < friendEmojiCount; e++) {
                    if (friendEmojis[e].shortcut == shortcut) {
                      foundEmoji = true;
                      pixelBytes = (uint8_t*)friendEmojis[e].pixels;
                      Serial.println("  MATCH in friendEmojis! Preparing to send...");
                      break;
                    }
                  }
                }

                if (foundEmoji && pixelBytes) {
                  // ALWAYS send custom emojis for now (simplified logic)
                  // TODO: Implement reliable receipt confirmation before optimizing
                  bool needsTransfer = true;

                  Serial.println("  ✓ Will send emoji to all peers (always send mode)");

                  // OLD LOGIC - disabled for reliability:
                  // Check if we need to send this emoji
                  // bool needsTransfer = false;
                  // if (dmTargetID.length() > 0) {
                  //   if (!wasEmojiSentToPeer(shortcut, dmTargetID)) {
                  //     needsTransfer = true;
                  //   }
                  // } else {
                  //   int peerCount = espNowManager.getPeerCount();
                  //   for (int p = 0; p < peerCount; p++) {
                  //     PeerDevice* peer = espNowManager.getPeer(p);
                  //     if (peer && !wasEmojiSentToPeer(shortcut, String(peer->deviceID))) {
                  //       needsTransfer = true;
                  //       break;
                  //     }
                  //   }
                  // }

                  // Only send chunks if needed
                  if (needsTransfer) {
                    Serial.println("  🚀 SENDING EMOJI CHUNKS FOR: " + shortcut);

                    // Show visual feedback on screen
                    canvas.fillRect(5, 107, 230, 23, TFT_PURPLE);
                    canvas.setTextSize(1);
                    canvas.setTextColor(TFT_BLACK);
                    String statusMsg = "Sending :" + shortcut + ":...";
                    canvas.drawString(statusMsg.c_str(), 10, 112);

                    // Send in 8 chunks of 64 bytes each (128 hex chars per chunk)
                    for (int chunk = 0; chunk < 8; chunk++) {
                      String chunkData = "EMOJI:" + shortcut + ":" + String(chunk) + ":";
                      for (int b = chunk * 64; b < (chunk + 1) * 64 && b < 512; b++) {
                        char hex[3];
                        sprintf(hex, "%02X", pixelBytes[b]);
                        chunkData += hex;
                      }

                      Serial.print("    📦 Sending chunk ");
                      Serial.print(chunk);
                      Serial.print("/8 (");
                      Serial.print(chunkData.length());
                      Serial.println(" bytes)");

                      // Send chunk as broadcast or DM
                      if (dmTargetID.length() > 0) {
                        messageHandler.sendDirect(dmTargetID.c_str(), chunkData.c_str());
                      } else {
                        messageHandler.sendBroadcast(chunkData.c_str(), chatCurrentChannel);
                      }
                      delay(150); // Increased to 150ms for better reliability

                      // Update progress on screen
                      canvas.fillRect(5, 107, 230, 23, TFT_PURPLE);
                      String progressMsg = ":" + shortcut + ": " + String(chunk + 1) + "/8";
                      canvas.drawString(progressMsg.c_str(), 10, 112);
                    }

                    // Mark emoji as sent to recipient(s)
                    if (dmTargetID.length() > 0) {
                      markEmojiSentToPeer(shortcut, dmTargetID);
                    } else {
                      // Mark for all current peers
                      int peerCount = espNowManager.getPeerCount();
                      for (int p = 0; p < peerCount; p++) {
                        PeerDevice* peer = espNowManager.getPeer(p);
                        if (peer) {
                          markEmojiSentToPeer(shortcut, String(peer->deviceID));
                        }
                      }
                    }

                    Serial.println("  ✅ Emoji transfer complete!");

                    // Show completion briefly
                    canvas.fillRect(5, 107, 230, 23, TFT_GREEN);
                    canvas.setTextColor(TFT_BLACK);
                    canvas.drawString(("Sent :" + shortcut + ":").c_str(), 10, 112);
                    delay(300);
                  }
                }

                if (!foundEmoji) {
                  Serial.println("  No match found in systemEmojis or friendEmojis");
                }

                i = endColon + 1;
              } else {
                i++;
              }
            } else {
              i++;
            }
          }

          // Check if user is allowing someone in
          if (chatInput == "allow" && knockerDeviceID.length() > 0) {
            // Get current room password to send to knocker
            String roomPassword = "";
            if (currentRoomIndex >= 0 && currentRoomIndex < activeRoomCount) {
              roomPassword = activeRooms[currentRoomIndex].password;
            }

            // Send allow response with password to knocker
            messageHandler.sendKnockResponse(knockerDeviceID.c_str(), true, roomPassword.c_str());

            // Send system message
            String allowMsg = String(knockerUsername) + " has been allowed in";
            messageHandler.addSystemMessage(allowMsg.c_str());

            // Clear knocker info
            knockerDeviceID = "";
            knockerUsername = "";

            chatInput = "";
            scrollPosition = 0;
          } else {
            // NOW send the text message AFTER emoji chunks are complete
            // Receiver will have all emojis ready and can display message immediately!
            if (dmTargetID.length() > 0) {
              // Send DM
              messageHandler.sendDirect(dmTargetID.c_str(), chatInput.c_str());
            } else {
              // Send broadcast
              messageHandler.sendBroadcast(chatInput.c_str(), chatCurrentChannel);
            }

            chatInput = "";
            scrollPosition = 0;
          }
        }
      } else if (key == 8 || key == 127) { // Backspace
        if (chatInput.length() > 0) {
          chatInput.remove(chatInput.length() - 1);
        }
      } else if (chatInput.length() > 0 && key >= 32 && key <= 126) {
        // Typing mode: add character, ignore hotkeys
        chatInput += key;

        // Check for emoji shortcuts after adding character
        if (chatInput.endsWith(":sb")) {
          chatInput = chatInput.substring(0, chatInput.length() - 3) + "\xF0\x9F\x8D\x93"; // 🍓 Strawberry
        } else if (chatInput.endsWith(":pine")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x8D\x8D"; // 🍍 Pineapple
        } else if (chatInput.endsWith(":cake")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x8D\xB0"; // 🍰 Cake
        } else if (chatInput.endsWith(":pill")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x8D\x89"; // Pill
        } else if (chatInput.endsWith(":atom")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x90\x9A"; // Atom
        } else if (chatInput.endsWith(":rad")) {
          chatInput = chatInput.substring(0, chatInput.length() - 4) + "\xE2\xAD\x90"; // Radioactive
        } else if (chatInput.endsWith(":mint")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x8D\xAC"; // 🍬 Peppermint
        } else if (chatInput.endsWith(":bio")) {
          chatInput = chatInput.substring(0, chatInput.length() - 4) + "\xF0\x9F\x94\xA5"; // Biohazard
        } else if (chatInput.endsWith(":syringe")) {
          chatInput = chatInput.substring(0, chatInput.length() - 8) + "\xF0\x9F\x92\x80"; // Syringe
        } else if (chatInput.endsWith(":rocket")) {
          chatInput = chatInput.substring(0, chatInput.length() - 7) + "\xF0\x9F\x9A\x80"; // 🚀 Rocket
        } else if (chatInput.endsWith(":dna")) {
          chatInput = chatInput.substring(0, chatInput.length() - 4) + "\xE2\x9A\xA1"; // DNA
        } else if (chatInput.endsWith(":flask")) {
          chatInput = chatInput.substring(0, chatInput.length() - 6) + "\xF0\x9F\x8E\xB5"; // Flask
        } else if (chatInput.endsWith(":coffee")) {
          chatInput = chatInput.substring(0, chatInput.length() - 7) + "\xE2\x98\x95"; // ☕ Coffee
        } else if (chatInput.endsWith(":alien")) {
          chatInput = chatInput.substring(0, chatInput.length() - 6) + "\xF0\x9F\x91\xBE"; // 👾 Invader
        } else if (chatInput.endsWith(":target")) {
          chatInput = chatInput.substring(0, chatInput.length() - 7) + "\xF0\x9F\x8E\xAE"; // Target
        } else if (chatInput.endsWith(":moon")) {
          chatInput = chatInput.substring(0, chatInput.length() - 5) + "\xF0\x9F\x8C\x99"; // 🌙 Moon
        } else if (chatInput.endsWith(":heart")) {
          chatInput = chatInput.substring(0, chatInput.length() - 6) + "\xE2\x9D\xA4"; // ❤️ Heart
        } else if (chatInput.endsWith(":gem")) {
          chatInput = chatInput.substring(0, chatInput.length() - 4) + "\xF0\x9F\x92\x8E"; // 💎 Diamond
        }
      } else if (chatInput.length() == 0) {
        // Navigation mode: only process hotkeys when NOT typing
        if (key == '`') {
          chatState = CHAT_SETTINGS;
          chatSettingsMenuIndex = 0;
        } else if (key == 9) { // Tab - DM hotkey
          if (espNowManager.getPeerCount() > 0) {
            chatState = CHAT_DM_SELECT;
            selectedUserIndex = 0;
          }
        } else if (key == '\\') { // Backslash - emoji picker
          // Reload emojis in case new ones arrived
          systemEmojisLoaded = false;
          loadSystemEmojis();
          chatState = CHAT_EMOJI_PICKER;
          selectedEmojiIndex = 0;
        } else if (key == '#') { // Rename current channel
          channelNameInput = channelNames[chatCurrentChannel];
          chatState = CHAT_RENAME_CHANNEL;
        } else if (key == '!' || key == '1') { // ! or 1 - Clear emoji send history (force resend)
          extern int sentEmojiHistoryCount;
          sentEmojiHistoryCount = 0;
          Serial.println("🔄 EMOJI SEND HISTORY CLEARED - All emojis will be resent!");
          // Show visual feedback
          canvas.fillRect(5, 107, 230, 23, TFT_ORANGE);
          canvas.setTextSize(1);
          canvas.setTextColor(TFT_BLACK);
          canvas.drawString("Emoji cache cleared!", 50, 112);
          delay(800);
          needsRedraw = true;  // Use flag instead of direct draw
        } else if (key == 27) { // ESC - exit DM mode
          dmTargetID = "";
          dmTargetUsername = "";
        } else if (key >= 32 && key <= 126) {
          // Start typing
          chatInput += key;
        }
      }
      break;
    }

    case CHAT_USER_LIST: {
      if (key == '`') {
        chatState = CHAT_MAIN;
      }
      break;
    }

    case CHAT_SETTINGS: {
      if (key == ';') {
        chatSettingsMenuIndex = (chatSettingsMenuIndex - 1 + 5) % 5;
      } else if (key == '.') {
        chatSettingsMenuIndex = (chatSettingsMenuIndex + 1) % 5;
      } else if (key == ',' && chatSettingsMenuIndex == 1) {
        // Left arrow - cycle through rooms
        int totalOptions = activeRoomCount + 1;
        currentRoomIndex = (currentRoomIndex - 1 + totalOptions) % totalOptions;
      } else if (key == '/' && chatSettingsMenuIndex == 1) {
        // Right arrow - cycle through rooms
        int totalOptions = activeRoomCount + 1;
        currentRoomIndex = (currentRoomIndex + 1) % totalOptions;
      } else if (key == '\n') {
        if (chatSettingsMenuIndex == 0) {
          // Change username
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
          usernameInput = prefs.getString("username", defaultUsername);
          prefs.end();
          chatState = CHAT_CHANGE_USERNAME;
        } else if (chatSettingsMenuIndex == 1) {
          // Switch Room - confirm selection
          if (currentRoomIndex < activeRoomCount) {
            // Reconnect to existing room
            String roomPassword = activeRooms[currentRoomIndex].password;
            String oldNetwork = String(securityManager.getNetworkName());

            if (securityManager.joinNetwork(roomPassword)) {
              String newNetwork = String(securityManager.getNetworkName());

              espNowManager.deinit();
              espNowManager.init(securityManager.getPMK());
              chatState = CHAT_MAIN;
              messageHandler.sendPresence();
              lastPresenceBroadcast = millis();
              lastNetworkName = newNetwork;
            }
          } else {
            // New Room - go to Room Key entry
            chatState = CHAT_JOIN_NETWORK;
            networkPasswordInput = "";
          }
        } else if (chatSettingsMenuIndex == 2) {
          // Room Radar
          chatState = CHAT_ROOM_RADAR;
          enterRoomRadar();
        } else if (chatSettingsMenuIndex == 3) {
          // Manage emojis
          selectedEmojiManagerIndex = 0;
          chatState = CHAT_EMOJI_MANAGER;
        } else if (chatSettingsMenuIndex == 4) {
          // Leave network - go to home menu
          exitLabChat();
          currentScreenNumber = 0;
          currentState = MAIN_MENU;
          extern void drawScreen(bool inverted);
          extern bool uiInverted;
          drawScreen(uiInverted);
          return;
        }
      } else if (key == '`') {
        // ESC from settings - go to home menu
        exitLabChat();
        currentScreenNumber = 0;
        currentState = MAIN_MENU;
        extern void drawScreen(bool inverted);
        extern bool uiInverted;
        drawScreen(uiInverted);
        return;
      }
      break;
    }

    case CHAT_CHANNEL_SWITCH: {
      // Total options = active rooms + "New Room"
      int totalOptions = activeRoomCount + 1;

      if (key == ',') { // Left
        currentRoomIndex = (currentRoomIndex - 1 + totalOptions) % totalOptions;
      } else if (key == '/') { // Right
        currentRoomIndex = (currentRoomIndex + 1) % totalOptions;
      } else if (key == '\n') { // Enter - select room
        if (currentRoomIndex < activeRoomCount) {
          // Reconnect to existing room
          String roomPassword = activeRooms[currentRoomIndex].password;
          String oldNetwork = String(securityManager.getNetworkName());

          if (securityManager.joinNetwork(roomPassword)) {
            String newNetwork = String(securityManager.getNetworkName());

            // Don't clear messages when switching rooms (user said "no")

            espNowManager.deinit();
            espNowManager.init(securityManager.getPMK());
            chatState = CHAT_MAIN;
            messageHandler.sendPresence();
            lastPresenceBroadcast = millis();
            lastNetworkName = newNetwork;
          }
        } else {
          // New Room - go to Room Key entry
          chatState = CHAT_JOIN_NETWORK;
          networkPasswordInput = "";
        }
      } else if (key == '`') {
        chatState = CHAT_SETTINGS;
      }
      break;
    }

    case CHAT_DM_SELECT: {
      if (key == ';') {
        int peerCount = espNowManager.getPeerCount();
        selectedUserIndex = (selectedUserIndex - 1 + peerCount) % peerCount;
      } else if (key == '.') {
        int peerCount = espNowManager.getPeerCount();
        selectedUserIndex = (selectedUserIndex + 1) % peerCount;
      } else if (key == '\n') {
        // Enter DM mode with selected user
        PeerDevice* peer = espNowManager.getPeer(selectedUserIndex);
        if (peer) {
          dmTargetID = String(peer->deviceID);
          dmTargetUsername = String(peer->username);
        }
        chatState = CHAT_MAIN;
      } else if (key == '`') {
        chatState = CHAT_MAIN;
      }
      break;
    }

    case CHAT_NETWORK_INFO: {
      if (key == '`') {
        chatState = CHAT_SETTINGS;
      }
      break;
    }

    case CHAT_CHANGE_USERNAME: {
      if (key == '\n') {
        if (usernameInput.length() > 0 && usernameInput.length() <= 15) {
          Preferences prefs;
          prefs.begin("labchat", false);
          prefs.putString("username", usernameInput);
          prefs.end();
          chatState = CHAT_SETTINGS;
        }
      } else if (key == 8 || key == 127) {
        if (usernameInput.length() > 0) {
          usernameInput.remove(usernameInput.length() - 1);
        }
      } else if (key == '`') {
        chatState = CHAT_SETTINGS;
      } else if (usernameInput.length() < 15 && key >= 32 && key <= 126) {
        usernameInput += key;
      }
      break;
    }

    case CHAT_RENAME_CHANNEL: {
      if (key == '\n') {
        if (channelNameInput.length() > 0 && channelNameInput.length() <= 15) {
          channelNames[chatCurrentChannel] = channelNameInput;
          chatState = CHAT_MAIN;
        }
      } else if (key == 8 || key == 127) {
        if (channelNameInput.length() > 0) {
          channelNameInput.remove(channelNameInput.length() - 1);
        }
      } else if (key == '`') {
        chatState = CHAT_MAIN;
      } else if (channelNameInput.length() < 15 && key >= 32 && key <= 126) {
        channelNameInput += key;
      }
      break;
    }

    case CHAT_EMOJI_PICKER: {
      // 2-row grid navigation (12 per row)
      int emojisPerRow = 12;
      int currentRow = selectedEmojiIndex / emojisPerRow;
      int currentCol = selectedEmojiIndex % emojisPerRow;

      if (key == ',') { // Left
        if (currentCol > 0) selectedEmojiIndex--;
      } else if (key == '/') { // Right
        if (currentCol < emojisPerRow - 1 && selectedEmojiIndex < systemEmojiCount - 1) selectedEmojiIndex++;
      } else if (key == ';') { // Up
        if (currentRow > 0) selectedEmojiIndex -= emojisPerRow;
      } else if (key == '.') { // Down
        if (selectedEmojiIndex + emojisPerRow < systemEmojiCount) selectedEmojiIndex += emojisPerRow;
      } else if (key == '\n') { // Enter - add emoji shortcut to chat input
        if (selectedEmojiIndex < systemEmojiCount) {
          chatInput += ":" + systemEmojis[selectedEmojiIndex].shortcut + ":";
        }
        chatState = CHAT_MAIN;
      } else if (key == '`') { // Back
        chatState = CHAT_MAIN;
      }
      break;
    }

    case CHAT_EMOJI_MANAGER: {
      if (key == ',') { // Left
        if (selectedEmojiManagerIndex % 4 > 0) selectedEmojiManagerIndex--;
      } else if (key == '/') { // Right
        if (selectedEmojiManagerIndex % 4 < 3 && selectedEmojiManagerIndex < 19) selectedEmojiManagerIndex++;
      } else if (key == ';') { // Up
        if (selectedEmojiManagerIndex >= 4) selectedEmojiManagerIndex -= 4;
      } else if (key == '.') { // Down
        if (selectedEmojiManagerIndex + 4 < 20) selectedEmojiManagerIndex += 4;
      } else if (key == 8 || key == 127) { // Delete - remove emoji
        if (selectedEmojiManagerIndex < systemEmojiCount) {
          // Delete the emoji file
          String filename = "/labchat/emojis/" + systemEmojis[selectedEmojiManagerIndex].shortcut + ".emoji";
          SD.remove(filename.c_str());

          // Reload system emojis
          systemEmojisLoaded = false;
          loadSystemEmojis();

          // Adjust selection if needed
          if (selectedEmojiManagerIndex >= systemEmojiCount && selectedEmojiManagerIndex > 0) {
            selectedEmojiManagerIndex--;
          }
        }
      } else if (key == '`') { // Back
        chatState = CHAT_SETTINGS;
      }
      break;
    }

    case CHAT_ROOM_RADAR: {
      if (key == 't' || key == 'T') {
        // Toggle tracking mode
        radarTrackingMode = !radarTrackingMode;
      } else if (key == ';' && !radarTrackingMode) { // Up
        if (selectedRadarIndex > 0) selectedRadarIndex--;
      } else if (key == '.' && !radarTrackingMode) { // Down
        if (selectedRadarIndex < nearbyDeviceCount - 1) selectedRadarIndex++;
      } else if (key == '\n' && !radarTrackingMode && nearbyDeviceCount > 0) { // Enter - go to lobby
        if (selectedRadarIndex < nearbyDeviceCount) {
          lobbyRoomName = nearbyDevices[selectedRadarIndex].roomName;
          exitRoomRadar();
          networkPasswordInput = "";

          // Auto-knock on entry
          messageHandler.sendPublicKnock(lobbyRoomName.c_str());
          lobbyKnockTime = millis();

          chatState = CHAT_LOBBY;
        }
      } else if (key == '`') { // Back
        exitRoomRadar();
        chatState = CHAT_SETTINGS;
      }
      break;
    }

    case CHAT_LOBBY: {
      if (key == '`') { // Esc - Cancel and go back to settings
        lobbyRoomName = "";
        lobbyKnockTime = 0;
        chatState = CHAT_SETTINGS;
      }
      // Just wait for response - no input needed
      break;
    }
  }

  // Let updateLabChat() handle redraws via needsRedraw flag
  // This prevents constant redraws on every key press (battery drain)
  needsRedraw = true;
}
