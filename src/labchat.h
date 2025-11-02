#ifndef LABCHAT_H
#define LABCHAT_H

#include <Arduino.h>
#include "config.h"

// LabCHAT states
enum LabChatState {
  CHAT_SETUP_PIN,         // First-time PIN setup
  CHAT_ENTER_PIN,         // Enter PIN to unlock
  CHAT_NETWORK_MENU,      // Create/Join network menu
  CHAT_CREATE_NETWORK,    // Create new network (password + name)
  CHAT_JOIN_NETWORK,      // Join existing network (password)
  CHAT_MAIN,              // Main chat interface
  CHAT_USER_LIST,         // Show connected users
  CHAT_SETTINGS,          // Chat settings (username, leave network)
  CHAT_CHANNEL_SWITCH,    // Channel switcher overlay
  CHAT_DM_SELECT,         // Direct message user selection
  CHAT_NETWORK_INFO,      // Network information display
  CHAT_CHANGE_USERNAME,   // Change username input
  CHAT_RENAME_CHANNEL,    // Rename current channel
  CHAT_EMOJI_PICKER,      // Emoji picker overlay
  CHAT_EMOJI_MANAGER      // Emoji manager (view/delete slots)
};

// LabCHAT functions
void enterLabChat();
void exitLabChat();
void handleLabChatNavigation(char key);
void updateLabChat();
void drawLabChat();

// State-specific draw functions
void drawPinSetup();
void drawPinEntry();
void drawNetworkMenu();
void drawCreateNetwork();
void drawJoinNetwork();
void drawMainChat();
void drawUserList();
void drawChatSettings();
void drawChannelSwitch();
void drawDMSelect();
void drawNetworkInfo();
void drawChangeUsername();
void drawRenameChannel();
void drawEmojiPicker();
void drawEmojiManager();

// Helper functions
void drawLabChatHeader(const char* subtitle = nullptr);
void drawTextInputBox(const char* prompt, String& input, bool isPassword = false);
void drawNavHint(const char* text, int x, int y);

// LabCHAT globals
extern LabChatState chatState;
extern String pinInput;
extern String networkPasswordInput;
extern String networkNameInput;
extern String chatInput;
extern String usernameInput;
extern String channelNameInput;
extern int scrollPosition;
extern int selectedUserIndex;
extern int selectedEmojiIndex;
extern int chatCurrentChannel;
extern bool chatActive;
extern unsigned long lastPresenceBroadcast;
extern String dmTargetID;  // Device ID for DM mode (empty = broadcast)
extern String dmTargetUsername;  // Username for DM display

// Global notification flag
extern bool hasUnreadMessages;

#endif
