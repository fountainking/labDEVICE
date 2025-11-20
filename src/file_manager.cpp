#include "file_manager.h"
#include "ui.h"
#include "settings.h"
#include "audio_manager.h"
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <AnimatedGIF.h>
// PNGdec removed - wasn't working properly, use JPG instead

// Audio playback variables (simplified - now using audio_manager)
String currentAudioPath = "";  // Non-static so main.cpp can access it
static unsigned long audioStartTime = 0;

SPIClass sdSPI(HSPI);

FileManagerState fmState = FM_FOLDER_VIEW;
String currentPath = "/";
FileInfo fileInfoList[50];
int fileCount = 0;
int selectedFileIndex = 0;
bool sdCardMounted = false;
bool showOperationsMenu = false;

// GIF playback variables
static bool gifPlaying = false;
static String currentGifPath = "";
static AnimatedGIF gif;
static File gifFile;
int gifYOffset = 0; // Non-static so main.cpp can use it
static bool fullScreenMode = true; // Always full screen for file viewing

// PNG SUPPORT REMOVED - wasn't working, adds 4KB static allocation
// Use WiFi Transfer's auto-optimizer to convert PNG -> JPG before uploading

// File operation clipboard
static String clipboardPath = "";
static String clipboardName = "";
static bool clipboardIsCut = false; // true = cut (move), false = copy

// File search
String searchQuery = "";
bool searchActive = false;

// Batch selection
bool fileSelected[50];  // Selection state for each file
int selectedCount = 0;  // Number of selected files

// PNG support removed - use JPG instead

// Forward declarations
void drawNavHint(const char* text, int x, int y);
void drawFolderView();

// ============================================================================
// REUSABLE UI HELPER FUNCTIONS (reduces 488 lines of duplicate dialog code!)
// ============================================================================

// Generic text input dialog - used for rename, create folder, etc.
String showTextInputDialog(String title, String defaultValue, String okLabel = "OK", String cancelLabel = "Cancel") {
  String input = defaultValue;
  bool cursorVisible = true;
  unsigned long lastBlink = millis();

  while (true) {
    // Draw dialog box
    canvas.fillRect(10, 35, 220, 70, TFT_WHITE);
    canvas.drawRect(10, 35, 220, 70, TFT_BLACK);
    canvas.drawRect(11, 36, 218, 68, TFT_BLACK);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString(title.c_str(), 20, 45);

    // Draw input field with cursor
    canvas.fillRect(20, 60, 200, 12, TFT_WHITE);
    canvas.setTextColor(TFT_BLACK);

    String displayName = input;
    if (displayName.length() > 28) {
      displayName = displayName.substring(displayName.length() - 28);
    }
    canvas.drawString(displayName.c_str(), 20, 60);

    // Draw blinking cursor
    if (cursorVisible) {
      int cursorX = 20 + (displayName.length() * 6);
      canvas.drawLine(cursorX, 60, cursorX, 70, TFT_BLACK);
    }

    // Cursor blinking
    if (millis() - lastBlink > 500) {
      cursorVisible = !cursorVisible;
      lastBlink = millis();
    }

    // Footer
    canvas.fillRect(20, 85, 200, 10, TFT_WHITE);
    canvas.setTextColor(TFT_DARKGREY);
    String footerText = "Enter=" + okLabel + "  `=" + cancelLabel;
    drawNavHint(footerText.c_str(), 40, 85);

    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();

      // Enter key = confirm
      if (status.enter) {
        return input;
      }

      // Delete key = backspace
      if (status.del && input.length() > 0) {
        input.remove(input.length() - 1);
        cursorVisible = true;
        lastBlink = millis();
      }

      // Character input
      for (auto key : status.word) {
        if (key == '`') {
          return "";  // Cancel
        } else if (key >= 32 && key <= 126 && input.length() < 50) {
          input += (char)key;
          cursorVisible = true;
          lastBlink = millis();
        }
      }
    }

    delay(10);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Generic confirmation dialog - used for delete, etc.
bool showConfirmDialog(String title, String message, String okLabel = "Yes", String cancelLabel = "No") {
  // Draw confirmation dialog
  canvas.fillRect(20, 40, 200, 60, TFT_WHITE);
  canvas.drawRect(20, 40, 200, 60, TFT_BLACK);
  canvas.drawRect(21, 41, 198, 58, TFT_BLACK);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString(title.c_str(), 30, 50);

  // Show message (truncate if needed)
  if (message.length() > 28) {
    message = message.substring(0, 25) + "...";
  }
  canvas.drawString(message.c_str(), 30, 65);

  canvas.setTextColor(TFT_DARKGREY);
  String footerText = "Enter=" + okLabel + "  `=" + cancelLabel;
  drawNavHint(footerText.c_str(), 60, 82);

  // Wait for confirmation
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();

      if (status.enter) {
        return true;  // Confirmed
      }

      for (auto key : status.word) {
        if (key == '`') {
          drawFolderView();
          return false;  // Cancelled
        }
      }
    }
    delay(10);
  }
}

// Helper function to draw navigation hints with yellow rounded rectangle background
void drawNavHint(const char* text, int x, int y) {
  // Measure text width
  canvas.setTextColor(TFT_BLACK);
  int textWidth = canvas.textWidth(text);
  int padding = 4;

  // Draw yellow rounded rectangle background
  canvas.fillRoundRect(x - padding, y - 2, textWidth + (padding * 2), 12, 3, TFT_WHITE);
  canvas.drawRoundRect(x - padding, y - 2, textWidth + (padding * 2), 12, 3, TFT_BLACK);

  // Draw black text on yellow background
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString(text, x, y);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Draw star icon (matches WiFi Transfer style!)
void drawStar(int x, int y, int size, uint16_t color) {
  // 5-pointed star using filled triangles
  // This creates the iconic star from the WiFi Transfer page
  float angle = -PI / 2; // Start at top
  int outerR = size;
  int innerR = size / 2;

  for (int i = 0; i < 5; i++) {
    float a1 = angle + (i * 2 * PI / 5);
    float a2 = angle + ((i + 0.5) * 2 * PI / 5);
    float a3 = angle + ((i + 1) * 2 * PI / 5);

    int x1 = x + outerR * cos(a1);
    int y1 = y + outerR * sin(a1);
    int x2 = x + innerR * cos(a2);
    int y2 = y + innerR * sin(a2);
    int x3 = x + outerR * cos(a3);
    int y3 = y + outerR * sin(a3);

    canvas.fillTriangle(x, y, x1, y1, x2, y2, color);
    canvas.fillTriangle(x, y, x2, y2, x3, y3, color);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Custom header for file manager - matches WiFi Transfer style!
void drawFileManagerHeader() {
  // Calculate header text width
  String displayPath = currentPath;
  if (displayPath.length() > 22) {
    displayPath = "..." + displayPath.substring(displayPath.length() - 19);
  }

  // Calculate total width: star (12px) + "FILES" (30px) + " - " (12px) + path + right padding (30px)
  int pathWidth = displayPath.length() * 6;
  int totalWidth = 12 + 30 + 12 + pathWidth + 30;

  // Left-aligned header rectangle with dynamic width - aligned with file list at x=18
  canvas.fillRoundRect(18, 8, totalWidth, 20, 10, TFT_WHITE);
  canvas.drawRoundRect(18, 8, totalWidth, 20, 10, TFT_BLACK);
  canvas.drawRoundRect(19, 9, totalWidth - 2, 18, 9, TFT_BLACK);

  // Draw golden/yellow star icon (TFT_GOLD = 0xFEA0, or use TFT_YELLOW)
  drawStar(30, 18, 6, 0xFEA0);

  // "FILES" text in black
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("FILES", 48, 14);

  // Show current path after FILES
  canvas.drawString("-", 83, 14);
  canvas.drawString(displayPath.c_str(), 93, 14);

  // Message notification indicator (heart icon in top right)
  extern bool hasUnreadMessages;
  if (hasUnreadMessages) {
    int heartX = 205;
    int heartY = 12;
    canvas.fillCircle(heartX, heartY + 2, 2, TFT_BLUE);
    canvas.fillCircle(heartX + 4, heartY + 2, 2, TFT_BLUE);
    canvas.fillTriangle(
      heartX - 2, heartY + 3,
      heartX + 6, heartY + 3,
      heartX + 2, heartY + 9,
      TFT_BLUE
    );
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}


void enterFileManager() {
  // Light gray background
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Initializing SD...", 70, 60);

  // SD card is already initialized in setup() - just check if it's mounted
  sdCardMounted = (SD.cardType() != CARD_NONE);

  if (!sdCardMounted) {
    canvas.fillScreen(TFT_WHITE);
    drawFileManagerHeader();

    // Error panel (white rounded rect like WiFi Transfer)
    canvas.fillRoundRect(20, 50, 200, 60, 12, TFT_WHITE);
    canvas.drawRoundRect(20, 50, 200, 60, 12, TFT_BLACK);
    canvas.drawRoundRect(21, 51, 198, 58, 11, TFT_BLACK);

    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("SD Error!", 70, 60);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("Insert SD card", 75, 85);

    // Back button
    canvas.fillRoundRect(85, 118, 70, 12, 6, TFT_WHITE);
    canvas.drawRoundRect(85, 118, 70, 12, 6, TFT_BLACK);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("`=Back", 95, 120);

    fileCount = 0;
    return;
  }
  
  sdCardMounted = true;
  fmState = FM_FOLDER_VIEW;
  currentPath = "/";
  selectedFileIndex = 0;
  
  loadFolder("/");
}

void loadFolder(String path) {
  fileCount = 0;

  // Clear batch selection
  for (int i = 0; i < 50; i++) {
    fileSelected[i] = false;
  }
  selectedCount = 0;

  // Check SD card is still mounted before trying to open
  if (SD.cardType() == CARD_NONE) {
    sdCardMounted = false;
    canvas.fillScreen(TFT_WHITE);
    drawFileManagerHeader();
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("SD Error!", 70, 60);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("SD card not found", 70, 85);
    delay(1500);
    return;
  }

  // Normalize path: remove trailing slash (except for root "/")
  String normalizedPath = path;
  if (normalizedPath.length() > 1 && normalizedPath.endsWith("/")) {
    normalizedPath = normalizedPath.substring(0, normalizedPath.length() - 1);
  }

  // Loading message with WiFi Transfer style
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  String loadMsg = "Opening: " + path;
  if (loadMsg.length() > 30) {
    loadMsg = loadMsg.substring(0, 27) + "...";
  }
  canvas.drawString(loadMsg.c_str(), 20, 65);
  delay(300);

  File root = SD.open(normalizedPath);

  if (!root) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Failed to open!", 75, 85);
    delay(1500);
    // Try to recover by going to root
    if (path != "/") {
      currentPath = "/";
      selectedFileIndex = 0;
      loadFolder("/");
    }
    return;
  }

  if (!root.isDirectory()) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Not a directory!", 70, 85);
    delay(1500);
    root.close();
    // Try to recover by going to root
    if (path != "/") {
      currentPath = "/";
      selectedFileIndex = 0;
      loadFolder("/");
    }
    return;
  }
  
  File file = root.openNextFile();
  while (file && fileCount < 50) {
    String fileName = String(file.name());

    // Get just the filename without full path
    int lastSlash = fileName.lastIndexOf('/');
    if (lastSlash >= 0) {
      fileName = fileName.substring(lastSlash + 1);
    }

    // Skip hidden files and system files
    if (fileName.startsWith(".") || fileName == "System Volume Information") {
      file.close();
      file = root.openNextFile();
      continue;
    }

    // Populate FileInfo structure
    fileInfoList[fileCount].name = fileName;
    fileInfoList[fileCount].isDirectory = file.isDirectory();
    fileInfoList[fileCount].size = file.size();
    fileInfoList[fileCount].modified = file.getLastWrite();

    if (file.isDirectory()) {
      fileInfoList[fileCount].type = TYPE_FOLDER;
    } else {
      fileInfoList[fileCount].type = getFileType(fileName);
    }

    fileCount++;
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  
  drawFolderView();
}

FileType getFileType(String filename) {
  filename.toLowerCase();

  if (filename.endsWith(".pdf")) return TYPE_PDF;
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg") ||
      filename.endsWith(".png") || filename.endsWith(".bmp")) return TYPE_IMAGE;
  if (filename.endsWith(".gif")) return TYPE_GIF;
  if (filename.endsWith(".mp4") || filename.endsWith(".avi")) return TYPE_VIDEO;
  if (filename.endsWith(".mp3") || filename.endsWith(".wav")) return TYPE_AUDIO;
  if (filename.endsWith(".txt") || filename.endsWith(".md") ||
      filename.endsWith(".csv") || filename.endsWith(".rtf")) return TYPE_TEXT;
  if (filename.endsWith(".obj")) return TYPE_MODEL;

  return TYPE_UNKNOWN;
}

String formatFileSize(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024) + "KB";
  } else {
    return String(bytes / (1024 * 1024)) + "MB";
  }
}

void drawFolderView() {
  // White background
  canvas.fillScreen(TFT_WHITE);

  // Draw header with star
  drawFileManagerHeader();

  // Show search box if searching
  if (searchActive) {
    canvas.fillRoundRect(5, 31, 230, 12, 6, TFT_WHITE);
    canvas.drawRoundRect(5, 31, 230, 12, 6, TFT_BLACK);
    canvas.setTextColor(TFT_BLACK);
    String displaySearch = "Search: " + searchQuery;
    if (displaySearch.length() > 35) {
      displaySearch = displaySearch.substring(0, 32) + "...";
    }
    canvas.drawString(displaySearch.c_str(), 10, 34);
  }

  // Main content panel - narrower, rounder, higher up
  canvas.fillRoundRect(15, 38, 210, 72, 15, TFT_WHITE);
  canvas.drawRoundRect(15, 38, 210, 72, 15, TFT_BLACK);
  canvas.drawRoundRect(16, 39, 208, 70, 14, TFT_BLACK);
  canvas.drawRoundRect(17, 40, 206, 68, 13, TFT_BLACK);

  // Build filtered file list if searching
  int filteredIndices[50];
  int filteredCount = 0;

  if (searchActive && searchQuery.length() > 0) {
    String lowerQuery = searchQuery;
    lowerQuery.toLowerCase();
    for (int i = 0; i < fileCount; i++) {
      String lowerName = fileInfoList[i].name;
      lowerName.toLowerCase();
      if (lowerName.indexOf(lowerQuery) >= 0) {
        filteredIndices[filteredCount++] = i;
      }
    }
  } else {
    // No filter, show all files
    for (int i = 0; i < fileCount; i++) {
      filteredIndices[i] = i;
    }
    filteredCount = fileCount;
  }

  if (filteredCount == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString(searchActive ? "No matches" : "Empty folder", 70, 75);
  } else {
    // Show up to 5 items from filtered list
    int startIdx = max(0, selectedFileIndex - 2);
    int endIdx = min(filteredCount, startIdx + 5);

    for (int i = startIdx; i < endIdx; i++) {
      int fileIdx = filteredIndices[i];  // Map to actual file index
      int yPos = 45 + ((i - startIdx) * 13);

      bool selected = (i == selectedFileIndex);

      // Check if this file is currently playing
      String fullPath = currentPath + "/" + fileInfoList[fileIdx].name;
      bool isPlaying = (selected && isAudioPlaying() &&
                       fullPath.endsWith(".mp3") &&
                       fullPath.equalsIgnoreCase(getCurrentMusicPath()));

      canvas.setTextSize(1);

      // Prepare display text first to calculate highlight width
      String displayName = fileInfoList[fileIdx].name;
      if (displayName.length() > 28) {
        displayName = displayName.substring(0, 25) + "...";
      }

      // Calculate text width (including arrow for folders and file size)
      int xStart = fileInfoList[fileIdx].isDirectory ? 32 : 22;
      int textWidth = displayName.length() * 6; // 6 pixels per character
      int arrowWidth = fileInfoList[fileIdx].isDirectory ? 10 : 0; // Arrow + spacing

      // Add file size width if it's a file
      int totalWidth = arrowWidth + textWidth + 6; // +6 for padding
      if (!fileInfoList[fileIdx].isDirectory) {
        String sizeStr = formatFileSize(fileInfoList[fileIdx].size);
        totalWidth += sizeStr.length() * 6 + 10; // size text + gap
      }

      // Highlight for selected item - matches text length
      // Blue if playing, yellow if just selected
      if (selected) {
        uint16_t highlightColor = isPlaying ? TFT_BLUE : TFT_YELLOW;
        canvas.fillRoundRect(20, yPos - 2, totalWidth, 11, 5, highlightColor);
      }

      // Red arrow for folders - white if playing (blue highlight), red otherwise
      if (fileInfoList[fileIdx].isDirectory) {
        uint16_t arrowColor = (selected && isPlaying) ? TFT_WHITE : TFT_RED;
        canvas.setTextColor(arrowColor);
        canvas.drawString(">", 22, yPos);
      }

      // Filename - white if playing (blue highlight), black otherwise
      uint16_t textColor = (selected && isPlaying) ? TFT_WHITE : TFT_BLACK;
      canvas.setTextColor(textColor);
      canvas.drawString(displayName.c_str(), xStart, yPos);

      // File size (right side) - white if playing, gray otherwise
      if (!fileInfoList[fileIdx].isDirectory) {
        String sizeStr = formatFileSize(fileInfoList[fileIdx].size);
        uint16_t sizeColor = (selected && isPlaying) ? TFT_WHITE : 0x7BEF;
        canvas.setTextColor(sizeColor);
        int sizeWidth = sizeStr.length() * 6;
        canvas.drawString(sizeStr.c_str(), 218 - sizeWidth, yPos);
      }
    }

    // Scroll indicators (small triangles)
    if (startIdx > 0) {
      canvas.fillTriangle(230, 40, 227, 44, 233, 44, TFT_BLACK);
    }
    if (endIdx < filteredCount) {
      canvas.fillTriangle(230, 108, 227, 104, 233, 104, TFT_BLACK);
    }
  }

  // Bottom navigation hints (WiFi Transfer style white rounded buttons)
  canvas.setTextSize(1);

  // Show selection count if files are selected for batch operations
  if (selectedCount > 0) {
    String selectionMsg = String(selectedCount) + " selected";
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString(selectionMsg.c_str(), 8, 125);
  }
  // Show clipboard status if file is cut/copied
  else if (clipboardPath.length() > 0) {
    String clipboardMsg = clipboardIsCut ? "Move: " : "Copy: ";
    clipboardMsg += clipboardName;
    if (clipboardMsg.length() > 25) {
      clipboardMsg = clipboardMsg.substring(0, 22) + "...";
    }
    canvas.setTextColor(clipboardIsCut ? TFT_RED : TFT_BLUE);
    canvas.drawString(clipboardMsg.c_str(), 8, 125);
  }
  // Otherwise show navigation hints - no rectangle, just text
  else {
    if (searchActive) {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Esc=Clear", 85, 125);
    } else {
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Space=Select", 82, 125);
    }
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawFileViewer() {
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();
  
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("File Viewer", 55, 30);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString("Coming soon!", 75, 70);
  
  canvas.setTextSize(1);
  drawNavHint("Press ` to go back", 60, 120);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawTextViewer(const String& path) {
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  
  // Title
  String title = "Text: " + path;
  if (title.length() > 36) title = title.substring(0, 36) + "...";
  canvas.drawString(title.c_str(), 6, 26);
  
  // Open file
  File f = SD.open(path.c_str(), FILE_READ);
  int16_t y = 40;
  const int16_t lineH = 10;
  const int16_t maxWChars = 38;
  
  if (!f) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Failed to open file", 6, y);
  } else {
    String line;
    while (f.available() && y < 115) {
      line = f.readStringUntil('\n');
      line.replace("\r", "");
      if ((int)line.length() > maxWChars) line = line.substring(0, maxWChars);
      canvas.drawString(line.c_str(), 6, y);
      y += lineH;
    }
    f.close();
  }
  
  // Footer
  drawNavHint("Press ` to go back", 60, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawImageViewer(const String& path) {
  canvas.fillScreen(TFT_WHITE);

  // Check file exists
  if (!SD.exists(path.c_str())) {
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(1);
    canvas.drawString("File not found", 60, 55);
    drawNavHint("Press ` to return", 65, 122);
    return;
  }

  // Open file and display
  File imgFile = SD.open(path.c_str());
  if (!imgFile) {
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(1);
    canvas.drawString("Cannot open file", 55, 50);
    drawNavHint("Press ` to return", 65, 122);
    return;
  }

  bool success = false;
  String lowerPath = path;
  lowerPath.toLowerCase();

  if (lowerPath.endsWith(".jpg") || lowerPath.endsWith(".jpeg")) {
    success = canvas.drawJpg(&imgFile, 0, 0, 240, 135);
    imgFile.close();
  } else if (lowerPath.endsWith(".png")) {
    // PNG SUPPORT REMOVED - show helpful message
    imgFile.close();
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("PNG not supported", 55, 40);

    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("Use WiFi Transfer to", 50, 60);
    canvas.drawString("upload this file - it will", 40, 72);
    canvas.drawString("auto-convert to JPG!", 50, 84);

    canvas.setTextColor(TFT_CYAN);
    canvas.drawString("Or convert manually to", 45, 100);
    canvas.drawString("JPG before uploading", 50, 112);

    drawNavHint("` Back", 95, 127);
    Serial.println("PNG support removed - use JPG instead");
    success = false;
  } else if (lowerPath.endsWith(".bmp")) {
    success = canvas.drawBmp(&imgFile, 0, 0, 240, 135);
    imgFile.close();
  }

  if (!success) {
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(1);
    canvas.drawString("Failed to display", 55, 50);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setTextSize(1);
    canvas.drawString("Image may be corrupted", 40, 65);
    canvas.drawString("or unsupported format", 40, 75);
  }

  // Show footer
  drawNavHint("Press ` to return", 65, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// GIF callback functions
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = SD.open(fname);
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen) iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0) return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > 240) iWidth = 240;
  
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y + gifYOffset; // Use calculated offset
  
  if (y >= 135) return; // Off screen
  
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent) {
        s[x] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;
  }
  
  if (pDraw->ucHasTransparency) {
    uint8_t c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    for (x = 0; x < iWidth; x++) {
      if (s[x] == ucTransparent) {
        continue;
      }
      c = s[x];
      usTemp[0] = usPalette[c];
      iCount = 1;
      while (x + iCount < iWidth && s[x + iCount] == c && s[x + iCount] != ucTransparent) {
        usTemp[iCount] = usPalette[c];
        iCount++;
      }
      canvas.pushImage(pDraw->iX + x, y, iCount, 1, usTemp);
      x += (iCount - 1);
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++) {
      usTemp[x] = usPalette[s[x]];
    }
    canvas.pushImage(pDraw->iX, y, iWidth, 1, usTemp);
  }
}

// PNG callback functions REMOVED - use JPG instead (saves 4KB RAM + code complexity)

void drawGifViewer(const String& path) {
  gifPlaying = true;
  currentGifPath = path;
  
  canvas.fillScreen(TFT_WHITE);
  
  gif.begin(GIF_PALETTE_RGB565_BE);
  
  if (gif.open(path.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    // Get GIF dimensions to center it on full screen
    int gifWidth = gif.getCanvasWidth();
    int gifHeight = gif.getCanvasHeight();
    
    // Calculate vertical centering for full 135px height
    gifYOffset = (135 - gifHeight) / 2;
    if (gifYOffset < 0) gifYOffset = 0;
    
    canvas.fillScreen(TFT_WHITE);
  } else {
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Failed to load GIF", 50, 60);
    gifPlaying = false;
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void updateGifPlayback() {
  if (!gifPlaying) return;
  
  int result = gif.playFrame(true, NULL);
  
  if (result == 0) {
    // End of GIF, loop it
    gif.reset();
  } else if (result < 0) {
    // Error occurred
    gifPlaying = false;
  }
  
  // Redraw footer to keep it visible
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  drawNavHint("Press ` to stop", 70, 122);
}

void stopGifPlayback() {
  if (gifPlaying) {
    gif.close();
  }
  gifPlaying = false;
  currentGifPath = "";
}

bool isGifPlaying() {
  return gifPlaying;
}

void drawPDFViewer(const String& path) {
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("PDF Viewer", 55, 50);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("PDF rendering not yet", 45, 75);
  canvas.drawString("implemented", 75, 90);

  canvas.setTextColor(TFT_DARKGREY);
  drawNavHint("Press ` to go back", 60, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Audio player functions (now use centralized audio_manager)
void setAudioVolume(int vol) {
  setMasterVolume(vol);
}

int getAudioVolume() {
  return getMasterVolume();
}

void audioVolumeUp() {
  masterVolumeUp();
}

void audioVolumeDown() {
  masterVolumeDown();
}

bool playAudioFile(const String& path) {
  // ALWAYS set currentAudioPath so we can retry with Enter key if it fails
  currentAudioPath = path;
  audioStartTime = millis();

  bool success = startMusicPlayback(path);
  return success;
}

void stopAudioPlayback() {
  stopMusicPlayback();
  currentAudioPath = "";
}

bool isAudioPlaying() {
  return isMusicPlaying();
}

void updateAudioPlayback() {
  updateMusicPlayback();
}

void drawAudioPlayer(const String& path) {
  canvas.fillScreen(TFT_WHITE);
  drawFileManagerHeader();

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_ORANGE);
  canvas.drawString("Audio Player", 50, 30);

  // Show filename
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  String filename = path;
  int lastSlash = filename.lastIndexOf('/');
  if (lastSlash >= 0) {
    filename = filename.substring(lastSlash + 1);
  }
  if (filename.length() > 35) {
    filename = filename.substring(0, 35) + "...";
  }
  canvas.drawString(filename.c_str(), 10, 55);

  // Play status
  canvas.setTextSize(1);
  if (isAudioPlaying()) {
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("Playing...", 85, 70);

    // Animated equalizer bars
    int barHeight[] = {3, 5, 7, 5, 3};
    for (int i = 0; i < 5; i++) {
      int animHeight = barHeight[i] + ((millis() / 100 + i) % 4);
      canvas.fillRect(80 + (i * 6), 85 - animHeight, 4, animHeight, TFT_GREEN);
    }

    // Playback time
    unsigned long playTime = (millis() - audioStartTime) / 1000;
    int minutes = playTime / 60;
    int seconds = playTime % 60;
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
    canvas.drawString(timeStr, 95, 95);
  } else {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Stopped", 90, 75);
  }

  // Controls
  canvas.setTextColor(TFT_DARKGREY);
  drawNavHint(";/.=Prev/Next `=Back", 50, 110);
  drawNavHint("Enter=Play/Stop", 65, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void showDeleteConfirmation() {
  if (fileCount == 0 || selectedFileIndex >= fileCount) return;

  // Use reusable dialog system (was 48 lines, now 3!)
  if (showConfirmDialog("Delete this file?", fileInfoList[selectedFileIndex].name)) {
    deleteCurrentFile();
  } else {
    drawFolderView();
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void cutFile() {
  if (fileCount == 0 || selectedFileIndex >= fileCount) return;

  // Store file info for cut operation
  if (currentPath.endsWith("/")) {
    clipboardPath = currentPath + fileInfoList[selectedFileIndex].name;
  } else {
    clipboardPath = currentPath + "/" + fileInfoList[selectedFileIndex].name;
  }
  clipboardName = fileInfoList[selectedFileIndex].name;
  clipboardIsCut = true;

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
  drawFolderView();
}

void copyFile() {
  if (fileCount == 0 || selectedFileIndex >= fileCount) return;

  // Store file info for copy operation
  if (currentPath.endsWith("/")) {
    clipboardPath = currentPath + fileInfoList[selectedFileIndex].name;
  } else {
    clipboardPath = currentPath + "/" + fileInfoList[selectedFileIndex].name;
  }
  clipboardName = fileInfoList[selectedFileIndex].name;
  clipboardIsCut = false;

  if (settings.soundEnabled) M5Cardputer.Speaker.tone(1200, 50);
  drawFolderView();
}

void pasteFile() {
  if (clipboardPath.length() == 0) return;

  // Build destination path
  String destPath;
  if (currentPath.endsWith("/")) {
    destPath = currentPath + clipboardName;
  } else {
    destPath = currentPath + "/" + clipboardName;
  }

  // Check if source and destination are the same
  if (clipboardPath == destPath) {
    canvas.fillRect(0, 110, 240, 15, TFT_WHITE);
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString("Already here!", 75, 112);
    delay(1000);
    drawFolderView();
    return;
  }

  bool success = false;

  if (clipboardIsCut) {
    // Move operation (rename)
    success = SD.rename(clipboardPath.c_str(), destPath.c_str());
    if (success) {
      clipboardPath = "";
      clipboardName = "";
    }
  } else {
    // Copy operation (read source, write destination)
    File sourceFile = SD.open(clipboardPath.c_str(), FILE_READ);
    if (sourceFile && !sourceFile.isDirectory()) {
      File destFile = SD.open(destPath.c_str(), FILE_WRITE);
      if (destFile) {
        uint8_t buf[512];
        while (sourceFile.available()) {
          int bytesRead = sourceFile.read(buf, sizeof(buf));
          destFile.write(buf, bytesRead);
        }
        destFile.close();
        success = true;
      }
      sourceFile.close();
    }
  }

  if (success) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
    loadFolder(currentPath);
  } else {
    canvas.fillRect(0, 110, 240, 15, TFT_WHITE);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Paste failed!", 75, 112);
    delay(1000);
    drawFolderView();
  }
}

void deleteCurrentFile() {
  if (fileCount == 0 || selectedFileIndex >= fileCount) return;

  String filePath;
  if (currentPath.endsWith("/")) {
    filePath = currentPath + fileInfoList[selectedFileIndex].name;
  } else {
    filePath = currentPath + "/" + fileInfoList[selectedFileIndex].name;
  }

  bool success = false;
  if (fileInfoList[selectedFileIndex].isDirectory) {
    success = SD.rmdir(filePath.c_str());
  } else {
    success = SD.remove(filePath.c_str());
  }

  if (success) {
    // Reload folder
    if (selectedFileIndex >= fileCount - 1 && selectedFileIndex > 0) {
      selectedFileIndex--;
    }
    loadFolder(currentPath);
  } else {
    // Show error briefly
    canvas.fillRect(0, 110, 240, 15, TFT_BLACK);
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Delete failed!", 70, 112);
    delay(1000);
    drawFolderView();
  }
}

void showBatchDeleteConfirmation() {
  if (selectedCount == 0) return;

  // Use reusable dialog system (was 45 lines, now 5!)
  String message = String(selectedCount) + " files";
  if (showConfirmDialog("Delete selected?", message)) {
    batchDeleteFiles();
  } else {
    drawFolderView();
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void batchDeleteFiles() {
  int deletedCount = 0;
  int failedCount = 0;

  // Delete all selected files
  for (int i = 0; i < fileCount; i++) {
    if (fileSelected[i]) {
      String filePath;
      if (currentPath.endsWith("/")) {
        filePath = currentPath + fileInfoList[i].name;
      } else {
        filePath = currentPath + "/" + fileInfoList[i].name;
      }

      bool success = false;
      if (fileInfoList[i].isDirectory) {
        success = SD.rmdir(filePath.c_str());
      } else {
        success = SD.remove(filePath.c_str());
      }

      if (success) {
        deletedCount++;
      } else {
        failedCount++;
      }
    }
  }

  // Show result
  if (deletedCount > 0) {
    if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
  }

  // Reload folder to show changes
  if (selectedFileIndex >= fileCount - deletedCount && selectedFileIndex > 0) {
    selectedFileIndex = 0;
  }
  loadFolder(currentPath);

  // Show brief summary if there were failures
  if (failedCount > 0) {
    canvas.fillRect(0, 110, 240, 15, TFT_WHITE);
    canvas.setTextColor(TFT_RED);
    String failMsg = String(failedCount) + " failed to delete";
    canvas.drawString(failMsg.c_str(), 55, 112);
    delay(1500);
    drawFolderView();
  }
}

void renameFile() {
  if (fileCount == 0 || selectedFileIndex >= fileCount) return;

  String oldName = fileInfoList[selectedFileIndex].name;

  // Use reusable dialog system (was 110 lines, now 20!)
  String newName = showTextInputDialog("Rename:", oldName);

  if (newName.length() > 0 && newName != oldName) {
    // Perform rename
    String oldPath;
    String newPath;

    if (currentPath.endsWith("/")) {
      oldPath = currentPath + oldName;
      newPath = currentPath + newName;
    } else {
      oldPath = currentPath + "/" + oldName;
      newPath = currentPath + "/" + newName;
    }

    if (SD.rename(oldPath.c_str(), newPath.c_str())) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
      loadFolder(currentPath);
    } else {
      canvas.fillRect(0, 110, 240, 15, TFT_WHITE);
      canvas.setTextColor(TFT_RED);
      canvas.drawString("Rename failed!", 70, 112);
      delay(1000);
      drawFolderView();
    }
  } else {
    drawFolderView();
  }
}

void createFolder() {
  // Use reusable dialog system (was 103 lines, now 17!)
  String folderName = showTextInputDialog("New Folder:", "", "Create");

  if (folderName.length() > 0) {
    // Create folder
    String newFolderPath;
    if (currentPath.endsWith("/")) {
      newFolderPath = currentPath + folderName;
    } else {
      newFolderPath = currentPath + "/" + folderName;
    }

    if (SD.mkdir(newFolderPath.c_str())) {
      if (settings.soundEnabled) M5Cardputer.Speaker.tone(1500, 100);
      loadFolder(currentPath);
    } else {
      canvas.fillRect(0, 110, 240, 15, TFT_WHITE);
      canvas.setTextColor(TFT_RED);
      canvas.drawString("Create failed!", 70, 112);
      delay(1000);
      drawFolderView();
    }
  } else {
    drawFolderView();
  }
}

void loadFile(String path) {
  FileType type = getFileType(path);

  if (type == TYPE_TEXT) {
    fmState = FM_FILE_VIEWER;
    drawTextViewer(path);
  } else if (type == TYPE_IMAGE) {
    fmState = FM_FILE_VIEWER;
    drawImageViewer(path);
  } else if (type == TYPE_GIF) {
    fmState = FM_FILE_VIEWER;
    drawGifViewer(path);
  } else if (type == TYPE_AUDIO) {
    // Toggle playback: if this file is playing, stop it; otherwise start it
    String currentlyPlaying = getCurrentMusicPath();
    if (isAudioPlaying() && path.equalsIgnoreCase(currentlyPlaying)) {
      // Stop playback if clicking the same file
      stopMusicPlayback();
    } else {
      // Start playback of new file
      playAudioFile(path);
    }
    drawFolderView();  // Redraw to show blue highlight on playing file
  } else if (type == TYPE_PDF) {
    fmState = FM_FILE_VIEWER;
    drawPDFViewer(path);
  } else {
    // Other file types - placeholder
    fmState = FM_FILE_VIEWER;
    drawFileViewer();
  }
}

void handleFileManagerNavigation(char key) {
  if (fmState == FM_FOLDER_VIEW) {
    static int lastSelectedIndex = -1;
    bool needsRedraw = false;

    if (key == ';') {
      if (selectedFileIndex > 0) {
        selectedFileIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);
        needsRedraw = (selectedFileIndex != lastSelectedIndex);
      }
    } else if (key == '.') {
      if (selectedFileIndex < fileCount - 1) {
        selectedFileIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);
        needsRedraw = (selectedFileIndex != lastSelectedIndex);
      }
    }

    // Only redraw if selection actually changed
    if (needsRedraw) {
      lastSelectedIndex = selectedFileIndex;
      drawFolderView();
    }
  } else if (fmState == FM_FILE_VIEWER) {
    // Navigate to previous/next file while viewing
    if (key == ';') {
      if (selectedFileIndex > 0) {
        stopGifPlayback();
        selectedFileIndex--;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(800, 50);

        // Load the new file
        String filePath;
        if (currentPath.endsWith("/")) {
          filePath = currentPath + fileInfoList[selectedFileIndex].name;
        } else {
          filePath = currentPath + "/" + fileInfoList[selectedFileIndex].name;
        }

        // Skip folders
        if (fileInfoList[selectedFileIndex].type == TYPE_FOLDER) {
          return;
        }

        loadFile(filePath);
      }
    } else if (key == '.') {
      if (selectedFileIndex < fileCount - 1) {
        stopGifPlayback();
        selectedFileIndex++;
        if (settings.soundEnabled) M5Cardputer.Speaker.tone(1000, 50);

        // Load the new file
        String filePath;
        if (currentPath.endsWith("/")) {
          filePath = currentPath + fileInfoList[selectedFileIndex].name;
        } else {
          filePath = currentPath + "/" + fileInfoList[selectedFileIndex].name;
        }

        // Skip folders
        if (fileInfoList[selectedFileIndex].type == TYPE_FOLDER) {
          return;
        }

        loadFile(filePath);
      }
    }
  }
}