#include "terminal.h"
#include "ui.h"
#include "settings.h"
#include "config.h"
#include "star_rain.h"
#include "ascii_art.h"
#include "file_manager.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <Preferences.h>

// Terminal state variables
String currentInput = "";
std::vector<OutputLine> outputBuffer;
std::vector<String> commandHistory;
int historyIndex = -1;
String currentDirectory = "/";
int scrollOffset = 0;
OutputLine currentLine;

void loadCommandHistory() {
  Preferences prefs;
  prefs.begin("terminal", true); // read-only
  int count = prefs.getInt("histCount", 0);
  for (int i = 0; i < count && i < TERMINAL_HISTORY_SIZE; i++) {
    String cmd = prefs.getString(("cmd" + String(i)).c_str(), "");
    if (cmd.length() > 0) {
      commandHistory.push_back(cmd);
    }
  }
  prefs.end();
}

void saveCommandHistory() {
  Preferences prefs;
  prefs.begin("terminal", false); // read-write
  prefs.putInt("histCount", commandHistory.size());
  for (int i = 0; i < commandHistory.size(); i++) {
    prefs.putString(("cmd" + String(i)).c_str(), commandHistory[i]);
  }
  prefs.end();
}

void enterTerminal() {
  currentInput = "";
  scrollOffset = 0;

  // Load command history from preferences
  if (commandHistory.size() == 0) {
    loadCommandHistory();
  }

  // SD card is already initialized in setup() - just check if it's mounted
  if (SD.cardType() == CARD_NONE) {
    addOutput("ERROR: SD card not found");
  }

  // Welcome banner - compact slant style with fire gradient
  const char* line1 = "  ___ _____ _   ___";
  const char* line2 = " / __|_   _/_\\ | _ \\";
  const char* line3 = " \\__ \\ | |/ _ \\|   /";
  const char* line4 = " |___/ |_/_/ \\_\\_|_\\";
  const char* line5 = "  ___  ___";
  const char* line6 = " / _ \\/ __|";
  const char* line7 = "| (_) \\__ \\";
  const char* line8 = " \\___/|___/";

  // Apply fire gradient to STAR section
  for (int i = 0; line1[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line1));
    terminalPrintColored(String(line1[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line2[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line2));
    terminalPrintColored(String(line2[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line3[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line3));
    terminalPrintColored(String(line3[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line4[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line4));
    terminalPrintColored(String(line4[i]), color);
  }
  terminalPrintln("");

  // OS section
  for (int i = 0; line5[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line5));
    terminalPrintColored(String(line5[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line6[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line6));
    terminalPrintColored(String(line6[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line7[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line7));
    terminalPrintColored(String(line7[i]), color);
  }
  terminalPrintln("");

  for (int i = 0; line8[i] != 0; i++) {
    uint16_t color = getGradientColor("fire", i, strlen(line8));
    terminalPrintColored(String(line8[i]), color);
  }
  // Add "Type 'help' for commands" on same line as last ASCII art line
  terminalPrintColored("  Type 'help' for commands", TFT_DARKGREY);
  terminalPrintln("");

  // Execute startup script if it exists
  if (SD.exists("/.profile")) {
    File startupFile = SD.open("/.profile", FILE_READ);
    if (startupFile) {
      while (startupFile.available()) {
        String line = startupFile.readStringUntil('\n');
        line.trim();
        // Skip empty lines and comments
        if (line.length() > 0 && !line.startsWith("#")) {
          executeCommand(line);
        }
      }
      startupFile.close();
    }
  }

  drawTerminal();
}

// Check if command is valid
bool isValidCommand(const String& cmd) {
  String trimmedCmd = cmd;
  trimmedCmd.trim();

  if (trimmedCmd.length() == 0) return false;

  // Extract just the command (first word)
  int spaceIndex = trimmedCmd.indexOf(' ');
  String command = spaceIndex > 0 ? trimmedCmd.substring(0, spaceIndex) : trimmedCmd;
  command.toLowerCase();

  // List of all valid commands
  return (command == "help" || command == "clear" || command == "cls" ||
          command == "ls" || command == "dir" || command == "cd" ||
          command == "pwd" || command == "cat" || command == "type" ||
          command == "rm" || command == "del" || command == "mkdir" ||
          command == "md" || command == "rmdir" || command == "rd" ||
          command == "free" || command == "mem" || command == "uptime" ||
          command == "reboot" || command == "restart" ||
          command == "wifi-scan" || command == "wifi-connect" ||
          command == "wifi-status" || command == "wifi-disconnect" ||
          command == "echo" || command == "stars" || command == "art" ||
          command == "colors" || command == "colortest" ||
          command == "img" || command == "view" || command == "xx" ||
          command == "run" || command == "script" || command == "edit" ||
          command == "scan" || command == "ping" || command == "arp" ||
          command == "trace" || command == "whois" || command == "dns" ||
          command == "netstat" || command == "mac" || command == "speed" ||
          command == "ip");
}

void drawTerminal() {
  // Clear entire screen
  canvas.fillScreen(TFT_BLACK);

  // Define layout zones with margins (Files app style)
  int pathBoxY = 8;  // Moved down from top
  int pathBoxH = 20; // Taller box
  int cmdBoxY = 108; // Moved up from bottom
  int cmdBoxH = 24;  // Taller box

  // Terminal content area with margins
  int terminalStartY = pathBoxY + pathBoxH + 2; // Content starts BELOW path box
  int terminalEndY = cmdBoxY - 3; // Stop ABOVE command box with margin
  int terminalMarginX = 15; // Left margin for text (aligned with box insets)

  // Draw terminal output buffer with AGGRESSIVE AUTO-SCROLL TO BOTTOM
  canvas.setTextSize(1);

  int lineHeight = 9;
  int maxVisibleLines = (terminalEndY - terminalStartY) / lineHeight; // Calculate available lines

  // Auto-scroll: always show most recent lines at bottom, aligned to bottom of content area
  int startLine = max(0, (int)outputBuffer.size() - maxVisibleLines + scrollOffset);
  int endLine = min((int)outputBuffer.size(), startLine + maxVisibleLines);

  // Calculate starting Y position to align bottom of text with terminalEndY
  int visibleLineCount = endLine - startLine;
  int yPos = terminalEndY - (visibleLineCount * lineHeight);

  for (int i = startLine; i < endLine; i++) {
    if (i >= 0 && i < outputBuffer.size()) {
      int xPos = terminalMarginX;
      // Render each colored segment in the line
      for (const auto& segment : outputBuffer[i].segments) {
        canvas.setTextColor(segment.color);
        canvas.drawString(segment.text.c_str(), xPos, yPos);
        xPos += segment.text.length() * 6; // 6 pixels per character
      }
      yPos += lineHeight;

      // Safety check: stop if we've somehow reached the command box
      if (yPos > terminalEndY) break;
    }
  }

  // Draw PATH BOX at top (yellow outline, narrower)
  canvas.fillRoundRect(10, pathBoxY, 220, pathBoxH, 8, TFT_BLACK);
  canvas.drawRoundRect(10, pathBoxY, 220, pathBoxH, 8, TFT_YELLOW);

  // Draw current directory path (just the path, no label)
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_YELLOW);
  String pathDisplay = currentDirectory;
  if (pathDisplay.length() > 34) {
    pathDisplay = "..." + pathDisplay.substring(pathDisplay.length() - 31);
  }
  canvas.drawString(pathDisplay.c_str(), 15, pathBoxY + 6);

  // Draw COMMAND LINE BOX at bottom (orange outline, narrower)
  canvas.fillRoundRect(10, cmdBoxY, 220, cmdBoxH, 8, TFT_BLACK);
  canvas.drawRoundRect(10, cmdBoxY, 220, cmdBoxH, 8, TFT_ORANGE);

  // Draw command line input inside bottom box
  int inputY = cmdBoxY + 8;

  // Use * as the prompt (asterisk represents command line)
  String prompt = "* ";
  String fullInput = prompt + currentInput;

  int maxCharsPerLine = 32; // Fits inside narrower command box (230px width - margins)

  // Define cycling gradient colors (yellow -> orange -> red -> purple -> blue -> cyan -> white -> back to yellow)
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

  // Helper function to get smooth gradient color for a character at position
  auto getInputGradientColor = [&](int charIndex, int totalChars, bool isValidCmd) -> uint16_t {
    if (isValidCmd) {
      return TFT_GREEN;  // Valid commands glow green
    }

    // Ping-pong gradient: goes forward then reverses (1→7→1 instead of 1→7→1→7)
    // Total cycle = 80 chars (40 forward + 40 reverse)
    int cycleLength = 80;
    int posInCycle = charIndex % cycleLength;
    float t;

    if (posInCycle < 40) {
      // First half: forward (0.0 to 1.0)
      t = (float)posInCycle / 40.0f;
    } else {
      // Second half: reverse (1.0 to 0.0)
      t = (float)(80 - posInCycle) / 40.0f;
    }

    // Map position to color in gradient array with smooth interpolation
    float colorPosition = t * (numColors - 1);
    int colorIndex1 = (int)colorPosition;
    int colorIndex2 = (colorIndex1 + 1) % numColors;
    float blend = colorPosition - colorIndex1;

    // Interpolate between two adjacent colors
    return interpolateColor(gradientColors[colorIndex1], gradientColors[colorIndex2], blend);
  };

  // Check if input wraps to second line
  if (fullInput.length() <= maxCharsPerLine) {
    // Single line - draw with gradient
    int xPos = 15; // Start inside command box with margin

    // Draw prompt character by character with gradient
    for (int i = 0; i < prompt.length(); i++) {
      canvas.setTextColor(getInputGradientColor(i, fullInput.length(), false));
      canvas.drawString(String(prompt[i]).c_str(), xPos, inputY);
      xPos += 6;
    }

    // Split input into command and arguments
    int spaceIndex = currentInput.indexOf(' ');
    String command = spaceIndex > 0 ? currentInput.substring(0, spaceIndex) : currentInput;
    String args = spaceIndex > 0 ? currentInput.substring(spaceIndex) : "";

    bool isValid = isValidCommand(currentInput);

    // Draw command character by character
    for (int i = 0; i < command.length(); i++) {
      canvas.setTextColor(getInputGradientColor(prompt.length() + i, fullInput.length(), isValid));
      canvas.drawString(String(command[i]).c_str(), xPos, inputY);
      xPos += 6;
    }

    // Draw arguments character by character with gradient
    for (int i = 0; i < args.length(); i++) {
      canvas.setTextColor(getInputGradientColor(prompt.length() + command.length() + i, fullInput.length(), false));
      canvas.drawString(String(args[i]).c_str(), xPos, inputY);
      xPos += 6;
    }

    // Draw cursor
    int cursorX = 10 + fullInput.length() * 6;
    if (millis() % 1000 < 500) {
      canvas.fillRect(cursorX, inputY, 6, 8, getInputGradientColor(fullInput.length(), fullInput.length(), false));
    }
  } else {
    // Auto-scroll input: only show the last N characters that fit
    String visibleInput = fullInput.substring(fullInput.length() - maxCharsPerLine);

    // Draw visible portion character by character
    int xPos = 10;
    for (int i = 0; i < visibleInput.length(); i++) {
      int actualIndex = fullInput.length() - maxCharsPerLine + i;
      bool isValid = (actualIndex >= prompt.length()) && isValidCommand(currentInput);
      canvas.setTextColor(getInputGradientColor(actualIndex, fullInput.length(), isValid));
      canvas.drawString(String(visibleInput[i]).c_str(), xPos, inputY);
      xPos += 6;
    }

    // Draw cursor at end of visible text
    int cursorX = 10 + visibleInput.length() * 6;
    if (millis() % 1000 < 500) {
      canvas.fillRect(cursorX, inputY, 6, 8, getInputGradientColor(fullInput.length(), fullInput.length(), false));
    }
  }

  // Draw scroll indicators (adjusted for new layout)
  if (startLine > 0) {
    canvas.fillTriangle(235, 25, 230, 30, 240, 30, TFT_CYAN);
  }
  if (endLine < outputBuffer.size()) {
    canvas.fillTriangle(235, 108, 230, 103, 240, 103, TFT_CYAN);
  }
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void handleTerminalInput(char key) {
  if (key == '\n' || key == '\r') {
    // Execute command
    if (currentInput.length() > 0) {
      // Use * for root, > for subdirectories in output
      String prompt = (currentDirectory == "/") ? "* " : currentDirectory + "> ";
      addOutput(prompt + currentInput);

      // Add to history
      if (commandHistory.size() >= TERMINAL_HISTORY_SIZE) {
        commandHistory.erase(commandHistory.begin());
      }
      commandHistory.push_back(currentInput);
      historyIndex = -1;

      // Save history to persistent storage
      saveCommandHistory();

      executeCommand(currentInput);
      currentInput = "";
      scrollOffset = 0; // Reset scroll to bottom
    }
    drawTerminal();
  } else if (key == '\b' || key == 127) {
    // Backspace
    if (currentInput.length() > 0) {
      currentInput = currentInput.substring(0, currentInput.length() - 1);
      drawTerminal();
    }
  } else if (key == 27) {
    // ESC - clear input
    currentInput = "";
    drawTerminal();
  } else if (key >= 32 && key <= 126) {
    // Printable character
    if (currentInput.length() < MAX_INPUT_LENGTH) {
      currentInput += key;

      // Calculate if input would wrap to second line
      String prompt = (currentDirectory == "/") ? "* " : currentDirectory + "> ";
      int totalInputChars = prompt.length() + currentInput.length();
      int maxCharsPerLine = 38; // 240px / 6px per char = 40, -2 for margins

      // Check if we're on the second line AND output buffer would cause it to go off-screen
      if (totalInputChars > maxCharsPerLine) {
        // Input has wrapped - check if we need to scroll output up
        int maxVisibleLines = 12;
        int currentOutputLines = outputBuffer.size();

        // If output buffer fills the screen, scroll it up by one to make room
        if (currentOutputLines >= maxVisibleLines && scrollOffset == 0) {
          scrollOffset = -1;
        }
      }

      drawTerminal();
    }
  }
}

void executeCommand(const String& cmd) {
  String trimmedCmd = cmd;
  trimmedCmd.trim();

  if (trimmedCmd.length() == 0) return;

  // Parse command and arguments
  int spaceIndex = trimmedCmd.indexOf(' ');
  String command = spaceIndex > 0 ? trimmedCmd.substring(0, spaceIndex) : trimmedCmd;
  String args = spaceIndex > 0 ? trimmedCmd.substring(spaceIndex + 1) : "";

  command.toLowerCase();

  // Execute commands
  if (command == "help") {
    cmd_help();
  } else if (command == "clear" || command == "cls") {
    cmd_clear();
  } else if (command == "ls" || command == "dir") {
    cmd_ls(args);
  } else if (command == "cd") {
    cmd_cd(args);
  } else if (command == "pwd") {
    cmd_pwd();
  } else if (command == "cat" || command == "type") {
    cmd_cat(args);
  } else if (command == "rm" || command == "del") {
    cmd_rm(args);
  } else if (command == "mkdir" || command == "md") {
    cmd_mkdir(args);
  } else if (command == "rmdir" || command == "rd") {
    cmd_rmdir(args);
  } else if (command == "free" || command == "mem") {
    cmd_free();
  } else if (command == "uptime") {
    cmd_uptime();
  } else if (command == "reboot" || command == "restart") {
    cmd_reboot();
  } else if (command == "wifi-scan") {
    cmd_wifi_scan();
  } else if (command == "wifi-connect") {
    // Parse ssid and password from args
    int firstQuote = args.indexOf('"');
    int secondQuote = args.indexOf('"', firstQuote + 1);
    int thirdQuote = args.indexOf('"', secondQuote + 1);
    int fourthQuote = args.indexOf('"', thirdQuote + 1);

    if (firstQuote >= 0 && secondQuote > firstQuote && thirdQuote > secondQuote && fourthQuote > thirdQuote) {
      String ssid = args.substring(firstQuote + 1, secondQuote);
      String password = args.substring(thirdQuote + 1, fourthQuote);
      cmd_wifi_connect(ssid, password);
    } else {
      addOutput("Usage: wifi-connect \"SSID\" \"password\"");
    }
  } else if (command == "wifi-status") {
    cmd_wifi_status();
  } else if (command == "wifi-disconnect") {
    cmd_wifi_disconnect();
  } else if (command == "echo") {
    cmd_echo(args);
  } else if (command == "stars") {
    cmd_stars();
  } else if (command == "art") {
    cmd_art(args);
  } else if (command == "colors" || command == "colortest") {
    cmd_colors();
  } else if (command == "img" || command == "view") {
    cmd_img(args);
  } else if (command == "xx") {
    cmd_ssh(args);
  } else if (command == "run" || command == "script") {
    cmd_run(args);
  } else if (command == "edit") {
    cmd_edit(args);
  } else if (command == "scan") {
    cmd_scan(args);
  } else if (command == "ping") {
    cmd_ping(args);
  } else if (command == "arp") {
    cmd_arp(args);
  } else if (command == "trace") {
    cmd_trace(args);
  } else if (command == "whois") {
    cmd_whois(args);
  } else if (command == "dns") {
    cmd_dns(args);
  } else if (command == "netstat") {
    cmd_netstat(args);
  } else if (command == "mac") {
    cmd_mac(args);
  } else if (command == "speed") {
    cmd_speed(args);
  } else if (command == "ip") {
    cmd_ip();
  } else {
    addOutput("Unknown command: " + command);
    addOutput("Type 'help' for available commands");
  }
}

void addOutput(const String& text) {
  // Word wrap: max 35 characters per line (220px box - 10px margins = 210px / 6px per char)
  const int maxLineChars = 35;

  if (text.length() <= maxLineChars) {
    // Text fits on one line
    currentLine.segments.push_back(ColoredText(text, TFT_YELLOW));
    outputBuffer.push_back(currentLine);
    currentLine = OutputLine();
  } else {
    // Need to wrap text across multiple lines
    String remaining = text;
    while (remaining.length() > 0) {
      if (remaining.length() <= maxLineChars) {
        // Last chunk
        currentLine.segments.push_back(ColoredText(remaining, TFT_YELLOW));
        outputBuffer.push_back(currentLine);
        currentLine = OutputLine();
        remaining = "";
      } else {
        // Find last space within maxLineChars, or just cut at maxLineChars
        int breakPoint = maxLineChars;
        int lastSpace = remaining.lastIndexOf(' ', maxLineChars);
        if (lastSpace > 0 && lastSpace > maxLineChars / 2) {
          // Use space for natural word break
          breakPoint = lastSpace;
        }

        // Add this chunk
        String chunk = remaining.substring(0, breakPoint);
        currentLine.segments.push_back(ColoredText(chunk, TFT_YELLOW));
        outputBuffer.push_back(currentLine);
        currentLine = OutputLine();

        // Move to next chunk (skip the space if we broke on one)
        if (lastSpace > 0 && lastSpace == breakPoint) {
          remaining = remaining.substring(breakPoint + 1);
        } else {
          remaining = remaining.substring(breakPoint);
        }
      }
    }
  }

  // Limit buffer size
  while (outputBuffer.size() > MAX_OUTPUT_LINES) {
    outputBuffer.erase(outputBuffer.begin());
  }

  // Auto-scroll to bottom when new output is added
  extern int scrollOffset;
  scrollOffset = 0;
}

void clearOutput() {
  outputBuffer.clear();
}

void scrollTerminalUp() {
  if (scrollOffset < 0) {
    scrollOffset++;
    drawTerminal();
  }
}

void scrollTerminalDown() {
  int maxVisibleLines = 12;
  if (outputBuffer.size() > maxVisibleLines && scrollOffset > -(int)(outputBuffer.size() - maxVisibleLines)) {
    scrollOffset--;
    drawTerminal();
  }
}

// Command implementations

void cmd_help() {
  addOutput("=== STAR OS ===");
  addOutput("");
  addOutput("Files:");
  addOutput("  ls,cd,pwd,cat,rm,");
  addOutput("  mkdir,rmdir,img");
  addOutput("");
  addOutput("System:");
  addOutput("  help,clear,free,");
  addOutput("  uptime,reboot");
  addOutput("");
  addOutput("WiFi:");
  addOutput("  wifi-scan,wifi-connect");
  addOutput("  wifi-status,wifi-disconnect");
  addOutput("");
  addOutput("Network:");
  addOutput("  scan - Port scanner");
  addOutput("  ping - Test connectivity");
  addOutput("  arp  - Find local devices");
  addOutput("  dns  - Lookup IP address");
  addOutput("  whois - Domain info");
  addOutput("  trace - Trace route");
  addOutput("  netstat - Connection info");
  addOutput("  mac  - Change MAC address");
  addOutput("  speed - Speed test");
  addOutput("");
  addOutput("Remote:");
  addOutput("  xx <host> - Telnet");
  addOutput("");
  addOutput("Scripts:");
  addOutput("  run <file> - Execute");
  addOutput("  edit <file> - Editor");
  addOutput("");
  addOutput("Fun:");
  addOutput("  echo,stars,art,colors");
  addOutput("");
  addOutput("Startup: /.profile");
  addOutput("Use <cmd> -h for help");
}

void cmd_clear() {
  clearOutput();
}

void cmd_ls(const String& path) {
  String targetPath = path.length() > 0 ? getAbsolutePath(path) : currentDirectory;

  File dir = SD.open(targetPath.c_str());
  if (!dir) {
    addOutput("ERROR: Cannot open " + targetPath);
    return;
  }

  if (!dir.isDirectory()) {
    addOutput("ERROR: Not a directory");
    dir.close();
    return;
  }

  addOutput("Directory: " + targetPath);

  File entry = dir.openNextFile();
  int fileCount = 0;
  int dirCount = 0;

  while (entry) {
    String name = String(entry.name());

    // Get just the filename (not full path)
    int lastSlash = name.lastIndexOf('/');
    if (lastSlash >= 0) {
      name = name.substring(lastSlash + 1);
    }

    if (entry.isDirectory()) {
      addOutput("  [" + name + "]");
      dirCount++;
    } else {
      String sizeStr = formatFileSize(entry.size());
      addOutput("  " + name + " (" + sizeStr + ")");
      fileCount++;
    }

    entry.close();
    entry = dir.openNextFile();
  }

  dir.close();
  addOutput(String(dirCount) + " dir(s), " + String(fileCount) + " file(s)");
}

void cmd_cd(const String& path) {
  if (path.length() == 0) {
    currentDirectory = "/";
    addOutput("Changed to /");
    return;
  }

  String newPath = getAbsolutePath(path);

  // Debug output
  addOutput("Trying: " + newPath);

  if (newPath == "/") {
    currentDirectory = "/";
    addOutput("Changed to /");
    return;
  }

  // Check if path exists
  if (!SD.exists(newPath.c_str())) {
    addOutput("ERROR: Path not found");
    addOutput("  " + newPath);
    return;
  }

  File dir = SD.open(newPath.c_str());
  if (!dir) {
    addOutput("ERROR: Cannot open path");
    addOutput("  " + newPath);
    return;
  }

  if (!dir.isDirectory()) {
    addOutput("ERROR: Not a directory");
    addOutput("  " + newPath);
    dir.close();
    return;
  }

  dir.close();
  currentDirectory = newPath;
  addOutput("Changed to " + currentDirectory);
}

void cmd_pwd() {
  addOutput(currentDirectory);
}

void cmd_cat(const String& filename) {
  if (filename.length() == 0) {
    addOutput("Usage: cat <filename>");
    return;
  }

  String filePath = getAbsolutePath(filename);
  File file = SD.open(filePath.c_str(), FILE_READ);

  if (!file) {
    addOutput("ERROR: Cannot open " + filePath);
    return;
  }

  if (file.isDirectory()) {
    addOutput("ERROR: Is a directory");
    file.close();
    return;
  }

  if (file.size() > 10000) {
    addOutput("WARNING: File is large (" + formatFileSize(file.size()) + ")");
    addOutput("Showing first 5KB...");
  }

  int bytesRead = 0;
  while (file.available() && bytesRead < 5000) {
    String line = file.readStringUntil('\n');
    addOutput(line);
    bytesRead += line.length();
  }

  file.close();
}

void cmd_rm(const String& filename) {
  if (filename.length() == 0) {
    addOutput("Usage: rm <filename>");
    return;
  }

  String filePath = getAbsolutePath(filename);

  if (!SD.exists(filePath.c_str())) {
    addOutput("ERROR: File not found");
    return;
  }

  File file = SD.open(filePath.c_str());
  if (file.isDirectory()) {
    addOutput("ERROR: Use rmdir for directories");
    file.close();
    return;
  }
  file.close();

  if (SD.remove(filePath.c_str())) {
    addOutput("Deleted: " + filePath);
  } else {
    addOutput("ERROR: Failed to delete file");
  }
}

void cmd_mkdir(const String& dirname) {
  if (dirname.length() == 0) {
    addOutput("Usage: mkdir <dirname>");
    return;
  }

  String dirPath = getAbsolutePath(dirname);

  if (SD.exists(dirPath.c_str())) {
    addOutput("ERROR: Already exists");
    return;
  }

  if (SD.mkdir(dirPath.c_str())) {
    addOutput("Created: " + dirPath);
  } else {
    addOutput("ERROR: Failed to create directory");
  }
}

void cmd_rmdir(const String& dirname) {
  if (dirname.length() == 0) {
    addOutput("Usage: rmdir <dirname>");
    return;
  }

  String dirPath = getAbsolutePath(dirname);

  if (!SD.exists(dirPath.c_str())) {
    addOutput("ERROR: Directory not found");
    return;
  }

  File dir = SD.open(dirPath.c_str());
  if (!dir.isDirectory()) {
    addOutput("ERROR: Not a directory");
    dir.close();
    return;
  }
  dir.close();

  if (SD.rmdir(dirPath.c_str())) {
    addOutput("Removed: " + dirPath);
  } else {
    addOutput("ERROR: Directory not empty or failed");
  }
}

void cmd_free() {
  size_t freeHeap = ESP.getFreeHeap();
  size_t totalHeap = ESP.getHeapSize();
  size_t freePSRAM = ESP.getFreePsram();
  size_t totalPSRAM = ESP.getPsramSize();

  addOutput("Memory Info:");
  addOutput("Heap:  " + String(freeHeap / 1024) + "KB / " + String(totalHeap / 1024) + "KB");
  addOutput("PSRAM: " + String(freePSRAM / 1024) + "KB / " + String(totalPSRAM / 1024) + "KB");

  // SD card info
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t usedSize = SD.usedBytes() / (1024 * 1024);
  addOutput("SD:    " + String((int)(cardSize - usedSize)) + "MB / " + String((int)cardSize) + "MB");
}

void cmd_uptime() {
  unsigned long uptimeMs = millis();
  unsigned long seconds = uptimeMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  String uptimeStr = "";
  if (days > 0) uptimeStr += String(days) + "d ";
  if (hours > 0) uptimeStr += String(hours) + "h ";
  if (minutes > 0) uptimeStr += String(minutes) + "m ";
  uptimeStr += String(seconds) + "s";

  addOutput("Uptime: " + uptimeStr);
}

void cmd_reboot() {
  addOutput("Rebooting in 2 seconds...");
  drawTerminal();
  delay(2000);
  ESP.restart();
}

void cmd_wifi_scan() {
  addOutput("Scanning WiFi networks...");
  drawTerminal();

  int n = WiFi.scanNetworks();

  if (n == 0) {
    addOutput("No networks found");
  } else {
    addOutput("Found " + String(n) + " networks:");
    for (int i = 0; i < min(n, 15); i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SEC";
      addOutput("  " + String(i + 1) + ". " + ssid + " (" + String(rssi) + "dBm) " + encryption);
    }
    if (n > 15) {
      addOutput("  ... and " + String(n - 15) + " more");
    }
  }
}

void cmd_wifi_connect(const String& ssid, const String& password) {
  addOutput("Connecting to: " + ssid);
  drawTerminal();

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addOutput("Connected!");
    addOutput("IP: " + WiFi.localIP().toString());
  } else {
    addOutput("ERROR: Connection failed");
    WiFi.disconnect();
  }
}

void cmd_wifi_status() {
  if (WiFi.status() == WL_CONNECTED) {
    addOutput("Status: Connected");
    addOutput("SSID: " + WiFi.SSID());
    addOutput("IP: " + WiFi.localIP().toString());
    addOutput("RSSI: " + String(WiFi.RSSI()) + " dBm");
    addOutput("MAC: " + WiFi.macAddress());
  } else {
    addOutput("Status: Disconnected");
  }
}

void cmd_wifi_disconnect() {
  WiFi.disconnect();
  addOutput("WiFi disconnected");
}

void cmd_echo(const String& text) {
  addOutput(text);
}

void cmd_stars() {
  // Trigger star rain terminal app
  // This will be handled in main.cpp when we detect terminalStarRainRequested
  extern bool terminalStarRainRequested;
  terminalStarRainRequested = true;
}

// Helper functions

String getAbsolutePath(const String& path) {
  String trimmedPath = path;
  trimmedPath.trim();

  if (trimmedPath.length() == 0) {
    return currentDirectory;
  }

  if (trimmedPath.startsWith("/")) {
    // Absolute path - remove trailing slash if present
    if (trimmedPath.length() > 1 && trimmedPath.endsWith("/")) {
      return trimmedPath.substring(0, trimmedPath.length() - 1);
    }
    return trimmedPath;
  }

  if (trimmedPath == ".") {
    return currentDirectory;
  }

  if (trimmedPath == "..") {
    if (currentDirectory == "/") {
      return "/";
    }
    int lastSlash = currentDirectory.lastIndexOf('/');
    if (lastSlash <= 0) {
      return "/";
    }
    return currentDirectory.substring(0, lastSlash);
  }

  // Relative path - remove trailing slash from path if present
  String cleanPath = trimmedPath;
  if (cleanPath.endsWith("/")) {
    cleanPath = cleanPath.substring(0, cleanPath.length() - 1);
  }

  if (currentDirectory == "/") {
    return "/" + cleanPath;
  } else {
    return currentDirectory + "/" + cleanPath;
  }
}

bool fileExists(const String& path) {
  return SD.exists(path.c_str());
}

bool isDirectory(const String& path) {
  File f = SD.open(path.c_str());
  if (!f) return false;
  bool isDir = f.isDirectory();
  f.close();
  return isDir;
}

void terminalPrintln(const String& text) {
  addOutput(text);
}

void terminalPrintColored(const String& text, uint16_t color) {
  // Add colored text to the current line being built
  currentLine.segments.push_back(ColoredText(text, color));
}

void cmd_colors() {
  addOutput("=== Terminal Color Test ===");
  addOutput("");
  addOutput("Input Color Behavior:");
  addOutput("- YELLOW: Invalid/unknown cmds");
  addOutput("- WHITE:  Valid commands");
  addOutput("");
  addOutput("Valid commands turn WHITE");
  addOutput("when recognized by terminal.");
  addOutput("");
  addOutput("Try typing:");
  addOutput("  'help' -> should be WHITE");
  addOutput("  'asdf' -> should be YELLOW");
  addOutput("  'ls'   -> should be WHITE");
  addOutput("");
  addOutput("Output is always YELLOW.");
  addOutput("Prompt is always YELLOW.");
}

void cmd_img(const String& filename) {
  if (filename.length() == 0) {
    addOutput("Usage: img <filename>");
    addOutput("Supports: .jpg, .png, .bmp");
    return;
  }

  String filePath = getAbsolutePath(filename);

  if (!SD.exists(filePath.c_str())) {
    addOutput("ERROR: File not found");
    return;
  }

  File file = SD.open(filePath.c_str());
  if (file.isDirectory()) {
    addOutput("ERROR: Is a directory");
    file.close();
    return;
  }
  file.close();

  // Get file extension
  String lowerPath = filePath;
  lowerPath.toLowerCase();

  // Clear screen and show image
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(1);

  bool success = false;

  if (lowerPath.endsWith(".jpg") || lowerPath.endsWith(".jpeg")) {
    // Load and display JPEG
    canvas.drawJpgFile(SD, filePath.c_str(), 0, 0, 240, 135);
    success = true;
  } else if (lowerPath.endsWith(".png")) {
    // Load and display PNG
    canvas.drawPngFile(SD, filePath.c_str(), 0, 0, 240, 135);
    success = true;
  } else if (lowerPath.endsWith(".bmp")) {
    // Load and display BMP
    canvas.drawBmpFile(SD, filePath.c_str(), 0, 0);
    success = true;
  } else {
    canvas.drawString("Unsupported format", 10, 60);
    canvas.drawString("Use .jpg, .png, or .bmp", 10, 70);
  }

  if (success) {
    // Show filename at top
    canvas.fillRect(0, 0, 240, 10, TFT_BLACK);
    String displayName = filename;
    if (displayName.length() > 38) {
      displayName = displayName.substring(0, 35) + "...";
    }
    canvas.drawString(displayName.c_str(), 2, 2);
  }

  // Show instructions at bottom
  canvas.fillRect(0, 125, 240, 10, TFT_BLACK);
  canvas.drawString("Press any key to return", 45, 127);

  // Wait for key press
  M5Cardputer.update();
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      auto status = M5Cardputer.Keyboard.keysState();
      if (status.word.size() > 0) {
        break;
      }
    }
    delay(10);
  }

  // Return to terminal
  addOutput("Image viewer closed");
  drawTerminal();
}

void cmd_ssh(const String& args) {
  if (args.length() == 0) {
    addOutput("Usage: xx <host>");
    addOutput("Example: xx 192.168.1.100");
    addOutput("Port 2323, password: xxxx");
    addOutput("Type 'exit' or ` to close");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    addOutput("Use wifi-connect first");
    return;
  }

  String hostname = args;
  hostname.trim();

  if (hostname.length() == 0) {
    addOutput("ERROR: Invalid hostname");
    return;
  }

  addOutput("Telnet: " + hostname + ":2323");
  addOutput("");
  addOutput("Connecting...");
  drawTerminal();

  // Create TCP client
  WiFiClient client;

  if (!client.connect(hostname.c_str(), 2323)) {
    addOutput("ERROR: Connection failed");
    addOutput("Make sure Telnet is enabled");
    return;
  }

  addOutput("Connected!");
  addOutput("");
  drawTerminal();

  // Wait for password prompt from server
  String serverPrompt = "";
  unsigned long promptTimeout = millis();
  while (millis() - promptTimeout < 5000) {  // 5 second timeout
    if (client.available()) {
      char c = client.read();
      if (c >= 32 && c <= 126) {
        serverPrompt += c;
      }
      if (serverPrompt.endsWith("Password: ")) {
        break;
      }
    }
    delay(10);
  }

  if (!serverPrompt.endsWith("Password: ")) {
    addOutput("ERROR: No password prompt");
    client.stop();
    return;
  }

  addOutput("");
  drawTerminal();

  // Password input with live asterisk masking
  String password = "";
  bool passwordEntered = false;
  String lastDisplayed = ""; // Track what we last drew to avoid flicker

  int cmdBoxY = 108;
  int cmdBoxH = 24;

  while (!passwordEntered && client.connected()) {
    String prompt = "Password: ";
    String stars = "";
    for (int i = 0; i < password.length(); i++) stars += "*";
    String currentDisplay = prompt + stars;

    // Only redraw if content changed
    if (currentDisplay != lastDisplayed) {
      // Clear just the text area inside the box
      canvas.fillRect(12, cmdBoxY + 2, 216, 20, TFT_BLACK);

      canvas.setTextColor(TFT_YELLOW);
      canvas.setTextSize(1);
      canvas.drawString(currentDisplay.c_str(), 15, cmdBoxY + 8);

      lastDisplayed = currentDisplay;
    }

    // Blinking cursor
    int cursorX = 15 + currentDisplay.length() * 6;
    static unsigned long lastBlink = 0;
    static bool cursorVisible = true;
    if (millis() - lastBlink > 500) {
      cursorVisible = !cursorVisible;
      lastBlink = millis();
      if (cursorVisible) {
        canvas.fillRect(cursorX, cmdBoxY + 8, 6, 8, TFT_YELLOW);
      } else {
        canvas.fillRect(cursorX, cmdBoxY + 8, 6, 8, TFT_BLACK);
      }
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

      if (status.enter) {
        passwordEntered = true;
      } else if (status.del) {
        if (password.length() > 0) {
          password.remove(password.length() - 1);
        }
      } else {
        for (auto key : status.word) {
          if (key >= 32 && key <= 126 && password.length() < 30) {
            password += (char)key;
          }
        }
      }
    }
    delay(10);
  }

  // Send password to server
  password += "\n";
  client.print(password);
  addOutput("");
  addOutput("Authenticating...");
  drawTerminal();

  // Wait for authentication response
  String authResponse = "";
  unsigned long authTimeout = millis();
  while (millis() - authTimeout < 5000) {
    if (client.available()) {
      char c = client.read();
      if (c >= 32 && c <= 126) {
        authResponse += c;
      } else if (c == '\n') {
        // Check if authenticated
        if (authResponse.indexOf("Authenticated") >= 0) {
          addOutput("Authenticated!");
          addOutput("Type 'exit' or ` to close");
          addOutput("");
          scrollOffset = 0;  // Auto-scroll to bottom
          drawTerminal();
          break;
        } else if (authResponse.indexOf("failed") >= 0) {
          addOutput("ERROR: Authentication failed");
          client.stop();
          return;
        }
        authResponse = "";
      }
    }
    delay(10);
  }

  if (!client.connected()) {
    addOutput("ERROR: Connection closed");
    return;
  }

  // Interactive telnet loop
  String inputLine = "";
  String currentPath = "~";  // Track current directory path
  bool connected = true;
  bool waitingForResponse = false;  // Track if waiting for Claude/server response
  bool inClaudeMode = false;  // Track if in Claude mode
  unsigned long thinkingStartTime = 0;

  // Command history
  std::vector<String> commandHistory;
  int historyIndex = -1;  // -1 = not browsing history, 0+ = index in history
  String tempInput = "";  // Store current input when browsing history
  unsigned long lastActivity = millis();

  while (connected && client.connected()) {
    M5Cardputer.update();

    // Read data from server and display it
    static unsigned long lastDrawTime = 0;
    static int charsSinceLastDraw = 0;
    bool needsRedraw = false;

    while (client.available()) {
      char c = client.read();

      // Stop thinking animation when data arrives
      waitingForResponse = false;

      // Handle telnet control codes (basic)
      if (c == '\xff') {
        // IAC (Interpret As Command) - skip next 2 bytes
        if (client.available()) client.read();
        if (client.available()) client.read();
        continue;
      }

      // Display received data from Mac
      if (c == '\n') {
        // Extract path from prompt - prompt comes BEFORE user input on same line
        // Format: "/path$ command" - we need to extract path before the $
        String lineStr = "";
        for (const auto& seg : currentLine.segments) {
          lineStr += seg.text;
        }

        // Try to find $ in the line (prompt marker)
        int dollarPos = lineStr.indexOf('$');

        if (dollarPos > 0) {
          // Extract everything before the $ as the prompt
          String promptPart = lineStr.substring(0, dollarPos);

          // Look for path after last slash or colon
          int colonPos = promptPart.lastIndexOf(':');
          int slashPos = promptPart.lastIndexOf('/');

          if (colonPos >= 0 && colonPos < promptPart.length() - 1) {
            // Format: "user@host:/path" - extract from colon
            currentPath = promptPart.substring(colonPos + 1);
          } else if (slashPos >= 0) {
            // Has a slash - likely a path like "/Users/name"
            currentPath = promptPart;
          } else {
            currentPath = promptPart;
          }

          // Clean up the path (remove any trailing spaces)
          currentPath.trim();

          if (currentPath.length() > 0) {
            currentDirectory = currentPath;  // Update global for path box display
          }
          inClaudeMode = false;  // Exited Claude mode
        } else if (lineStr.indexOf("Claude>") >= 0) {
          inClaudeMode = true;  // Entered Claude mode
        }

        addOutput("");
        scrollOffset = 0;  // Auto-scroll to bottom on new output
        drawTerminal();
        lastDrawTime = millis();
        charsSinceLastDraw = 0;
      } else if (c == '\r') {
        // Skip carriage returns
      } else if (c == 27) {
        // ESC - start of ANSI escape sequence, skip until we hit 'm' or other terminator
        while (client.available()) {
          char next = client.read();
          if (next >= 64 && next <= 126) break;  // Sequence terminator
        }
      } else if (c >= 32 && c <= 126) {
        // Add printable character to current line with smooth gradient (green-yellow)
        // Use same interpolation algorithm as main terminal
        static uint16_t gradientColors[] = {
          0x0300,  // Dark green
          0x0400,  // Green
          0x0500,  // Bright green
          0x25E0,  // Yellow-green
          0x65E0,  // More yellow-green
          0xA5E0,  // Even more yellow
          0xFFE0,  // Yellow
        };
        static int numColors = 7;
        static int charIndexInGradient = 0;

        // Wrap text at 40 characters
        int lineLength = 0;
        for (const auto& seg : currentLine.segments) {
          lineLength += seg.text.length();
        }

        if (lineLength >= 40) {
          addOutput("");  // Start new line
          // Don't reset charIndexInGradient - let gradient continue across lines
        }

        // Calculate smooth ping-pong gradient (forward then reverse)
        int cycleLength = 80;
        int posInCycle = charIndexInGradient % cycleLength;
        float t;

        if (posInCycle < 40) {
          // First half: forward (0.0 to 1.0)
          t = (float)posInCycle / 40.0f;
        } else {
          // Second half: reverse (1.0 to 0.0)
          t = (float)(80 - posInCycle) / 40.0f;
        }

        float colorPosition = t * (numColors - 1);
        int colorIndex1 = (int)colorPosition;
        int colorIndex2 = (colorIndex1 + 1) % numColors;
        float blend = colorPosition - colorIndex1;

        // Interpolate between two adjacent colors
        uint16_t charColor = interpolateColor(gradientColors[colorIndex1], gradientColors[colorIndex2], blend);

        currentLine.segments.push_back(ColoredText(String(c), charColor));
        charIndexInGradient++;
        charsSinceLastDraw++;
        needsRedraw = true;
      } else if (c == 8 || c == 127) {
        // Backspace - remove last character from current line if any
        if (currentLine.segments.size() > 0) {
          auto& lastSeg = currentLine.segments.back();
          if (lastSeg.text.length() > 0) {
            lastSeg.text.remove(lastSeg.text.length() - 1);
            if (lastSeg.text.length() == 0) {
              currentLine.segments.pop_back();
            }
          }
        }
        needsRedraw = true;
      }

      lastActivity = millis();
    }

    // Batch screen updates: redraw every 100ms OR every 50 chars (whichever comes first)
    // OR immediately if we have a prompt waiting (currentLine has content and no more data available)
    bool hasPromptWaiting = (currentLine.segments.size() > 0) && !client.available();
    if (needsRedraw && (millis() - lastDrawTime > 100 || charsSinceLastDraw > 50 || hasPromptWaiting)) {
      drawTerminal();
      lastDrawTime = millis();
      charsSinceLastDraw = 0;
    }

    // Handle M5 keyboard input
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

      // Check for backtick to exit
      if (status.word.size() > 0 && status.word[0] == '`') {
        connected = false;
        break;
      }

      // Check for Fn key combinations
      // Fn+, (left) and Fn+/ (right) = scroll terminal window
      // Fn+; (up) and Fn+. (down) = navigate command history
      if (status.fn && status.word.size() > 0) {
        if (status.word[0] == ',') {
          // Fn+, (left) = Scroll terminal DOWN
          int maxVisibleLines = 12;
          if (outputBuffer.size() > maxVisibleLines && scrollOffset > -(int)(outputBuffer.size() - maxVisibleLines)) {
            scrollOffset--;
            drawTerminal();
          }
          continue;
        } else if (status.word[0] == '/') {
          // Fn+/ (right) = Scroll terminal UP
          if (scrollOffset < 0) {
            scrollOffset++;
            drawTerminal();
          }
          continue;
        } else if (status.word[0] == ';') {
          // Fn+; (up) = Previous command in history
          if (commandHistory.size() > 0) {
            // First time pressing up - save current input
            if (historyIndex == -1) {
              tempInput = inputLine;
              historyIndex = commandHistory.size() - 1;
            } else if (historyIndex > 0) {
              historyIndex--;
            }
            inputLine = commandHistory[historyIndex];
          }
          continue;
        } else if (status.word[0] == '.') {
          // Fn+. (down) = Next command in history
          if (historyIndex >= 0) {
            if (historyIndex < (int)commandHistory.size() - 1) {
              historyIndex++;
              inputLine = commandHistory[historyIndex];
            } else {
              // Back to current input
              historyIndex = -1;
              inputLine = tempInput;
            }
          }
          continue;
        }
      }

      if (status.enter) {
        // Send complete command to server when Enter is pressed
        if (inputLine == "exit" || inputLine == "quit") {
          connected = false;
          break;
        }

        // Add to command history (skip empty commands and duplicates)
        if (inputLine.length() > 0) {
          // Don't add if it's the same as the last command
          if (commandHistory.size() == 0 || commandHistory.back() != inputLine) {
            commandHistory.push_back(inputLine);
            // Limit history to 50 commands
            if (commandHistory.size() > 50) {
              commandHistory.erase(commandHistory.begin());
            }
          }
        }

        // Optimistically update path for cd commands BEFORE sending to server
        if (inputLine.startsWith("cd ")) {
          String cdArg = inputLine.substring(3);
          cdArg.trim();

          if (cdArg == "..") {
            // Go up one directory
            int lastSlash = currentDirectory.lastIndexOf('/');
            if (lastSlash > 0) {
              currentDirectory = currentDirectory.substring(0, lastSlash);
            } else if (lastSlash == 0) {
              currentDirectory = "/";
            }
          } else if (cdArg.startsWith("/")) {
            // Absolute path
            currentDirectory = cdArg;
          } else if (cdArg.startsWith("~")) {
            // Home directory
            currentDirectory = cdArg;
          } else if (cdArg.length() > 0) {
            // Relative path
            if (currentDirectory.endsWith("/")) {
              currentDirectory += cdArg;
            } else {
              currentDirectory += "/" + cdArg;
            }
          }
        }

        // Send command to Mac (use \r for telnet protocol)
        client.print(inputLine + "\r");

        // If user typed "claude", send another enter to start the session
        if (inputLine == "claude") {
          delay(100);  // Brief delay to let server process
          client.print("\r");
        }

        // Start thinking animation ONLY if in Claude mode
        if (inClaudeMode && inputLine.length() > 0) {
          waitingForResponse = true;
          thinkingStartTime = millis();
        }

        // Clear input and redraw
        inputLine = "";
        historyIndex = -1;  // Reset history navigation
        tempInput = "";
        scrollOffset = 0;  // Auto-scroll to bottom
        canvas.fillRect(0, 117, 240, 18, TFT_BLACK);
        drawTerminal();

        lastActivity = millis();
      } else if (status.del) {
        // Backspace
        if (inputLine.length() > 0) {
          inputLine.remove(inputLine.length() - 1);
          // Reset history navigation when deleting
          historyIndex = -1;
          tempInput = "";
        }

        // Redraw terminal (no need to scroll for wrapping - command box handles it)
        drawTerminal();
      } else {
        // Add character to input buffer (but don't send to server yet!)
        for (auto key : status.word) {
          if (key >= 32 && key <= 126 && inputLine.length() < 120) {
            inputLine += (char)key;
            // Reset history navigation when typing new characters
            historyIndex = -1;
            tempInput = "";
          }
        }

        // Redraw terminal (no need to scroll for wrapping - command box handles it)
        drawTerminal();

        lastActivity = millis();
      }
    }

    // Box dimensions (defined here for input positioning)
    int pathBoxY = 8;
    int pathBoxH = 20;
    int cmdBoxY = 108;
    int cmdBoxH = 24;

    // Update path display with marquee scrolling for long paths
    static String lastPath = "";
    static int pathScrollPos = 0;
    static unsigned long lastScrollTime = 0;
    static unsigned long pauseTimer = 0;
    static bool paused = false;

    canvas.fillRect(12, pathBoxY + 2, 216, 16, TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW);

    String fullPath = currentDirectory;
    const int maxVisibleChars = 34;

    // Reset scroll if path changed
    if (fullPath != lastPath) {
      lastPath = fullPath;
      pathScrollPos = 0;
      paused = true;
      pauseTimer = millis();
      lastScrollTime = millis();
    }

    String displayPath;
    if (fullPath.length() <= maxVisibleChars) {
      // Path fits - no scrolling needed
      displayPath = fullPath;
    } else {
      // Path is long - scroll smoothly
      if (paused) {
        // Pause at start for 2 seconds before scrolling
        if (millis() - pauseTimer > 2000) {
          paused = false;
          lastScrollTime = millis();
        }
        displayPath = fullPath.substring(pathScrollPos, pathScrollPos + maxVisibleChars);
      } else {
        // Scroll one character every 150ms
        if (millis() - lastScrollTime > 150) {
          pathScrollPos++;

          // Reached the end - pause and reset
          if (pathScrollPos >= fullPath.length() - maxVisibleChars) {
            pathScrollPos = 0;
            paused = true;
            pauseTimer = millis();
          }

          lastScrollTime = millis();
        }
        displayPath = fullPath.substring(pathScrollPos, pathScrollPos + maxVisibleChars);
      }
    }

    canvas.drawString(displayPath.c_str(), 15, pathBoxY + 6);

    // Clear input area inside command box (boxes already drawn by drawTerminal)
    canvas.fillRect(12, cmdBoxY + 2, 216, 20, TFT_BLACK);

    // Use * prompt like main terminal (no path in command line)
    String promptText = "* ";

    // 5-color gradient matching main terminal style (green to yellow)
    static uint16_t inputGradientColors[] = {
      0x0400,  // Green
      0x0500,  // Bright green
      0x65E0,  // Yellow-green
      0xA5E0,  // Light yellow-green
      0xFFE0,  // Yellow
    };
    static int numInputColors = 5;

    // Lambda for smooth gradient color calculation (ping-pong: forward then reverse)
    auto getInputGradientColor = [&](int charIndex) -> uint16_t {
      // Ping-pong gradient: goes forward then reverses
      // Total cycle = 80 chars (40 forward + 40 reverse)
      int cycleLength = 80;
      int posInCycle = charIndex % cycleLength;
      float t;

      if (posInCycle < 40) {
        // First half: forward (0.0 to 1.0)
        t = (float)posInCycle / 40.0f;
      } else {
        // Second half: reverse (1.0 to 0.0)
        t = (float)(80 - posInCycle) / 40.0f;
      }

      float colorPosition = t * (numInputColors - 1);
      int colorIndex1 = (int)colorPosition;
      int colorIndex2 = (colorIndex1 + 1) % numInputColors;
      float blend = colorPosition - colorIndex1;
      return interpolateColor(inputGradientColors[colorIndex1], inputGradientColors[colorIndex2], blend);
    };

    String fullInputWithPath = promptText + inputLine;

    // Add animated thinking ellipsis ONLY if in Claude mode and waiting for response
    if (inClaudeMode && waitingForResponse) {
      unsigned long elapsed = millis() - thinkingStartTime;
      int dotCount = (elapsed / 500) % 4;  // 0-3 dots, cycles every 2 seconds
      String dots = "";
      for (int i = 0; i < dotCount; i++) {
        dots += ".";
      }
      fullInputWithPath = promptText + dots;
    }

    int xPos = 15; // Inside narrower command box
    int yPos = cmdBoxY + 8;
    int maxCharsPerLine = 32; // Fits inside narrower command box

    if (fullInputWithPath.length() <= maxCharsPerLine) {
      // Single line with gradient - draw inside command box
      for (int i = 0; i < fullInputWithPath.length(); i++) {
        canvas.setTextColor(getInputGradientColor(i));
        canvas.drawChar(fullInputWithPath[i], xPos, yPos);
        xPos += 6;
      }
      if (!waitingForResponse && millis() % 1000 < 500) {
        canvas.fillRect(xPos, yPos, 6, 8, 0xFFE0);
      }
    } else {
      // Auto-scroll: show last N characters that fit
      String visibleInput = fullInputWithPath.substring(fullInputWithPath.length() - maxCharsPerLine);
      for (int i = 0; i < visibleInput.length(); i++) {
        int actualIndex = fullInputWithPath.length() - maxCharsPerLine + i;
        canvas.setTextColor(getInputGradientColor(actualIndex));
        canvas.drawChar(visibleInput[i], xPos, yPos);
        xPos += 6;
      }
      if (!waitingForResponse && millis() % 1000 < 500) {
        canvas.fillRect(xPos, yPos, 6, 8, 0xFFE0);
      }
    }

    // Timeout check (5 minutes of no activity)
    if (millis() - lastActivity > 300000) {
      addOutput("");
      addOutput("Timeout: No activity");
      connected = false;
    }

    delay(10);
  }

  // Clean up and restore local directory
  client.stop();

  // Restore local terminal directory
  currentDirectory = "/";  // Reset to root or restore saved local path

  addOutput("");
  addOutput("Telnet session closed");
  drawTerminal();
}

void cmd_run(const String& filename) {
  if (filename.length() == 0) {
    addOutput("Usage: run <script>");
    addOutput("Example: run test.sh");
    return;
  }

  String filePath = getAbsolutePath(filename);
  File file = SD.open(filePath.c_str(), FILE_READ);

  if (!file) {
    addOutput("ERROR: Cannot open " + filePath);
    return;
  }

  if (file.isDirectory()) {
    addOutput("ERROR: Is a directory");
    file.close();
    return;
  }

  addOutput("Running: " + filePath);
  addOutput("");

  // Read and execute each line
  int lineNum = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    lineNum++;

    // Skip empty lines and comments
    if (line.length() == 0 || line.startsWith("#")) {
      continue;
    }

    // Execute the command
    addOutput("$ " + line);
    executeCommand(line);
  }

  file.close();
  addOutput("");
  addOutput("Script completed");
}

void cmd_edit(const String& filename) {
  if (filename.length() == 0) {
    addOutput("Usage: edit <file>");
    addOutput("Creates or edits a file");
    addOutput("Line editor - backtick to exit");
    return;
  }

  String filePath = getAbsolutePath(filename);

  // Check if file exists and is not a directory
  bool fileExists = SD.exists(filePath.c_str());
  if (fileExists) {
    File checkFile = SD.open(filePath.c_str());
    if (checkFile.isDirectory()) {
      addOutput("ERROR: Is a directory");
      checkFile.close();
      return;
    }
    checkFile.close();
  }

  // Clear screen for editor
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_YELLOW);

  // Load existing file content
  std::vector<String> lines;
  if (fileExists) {
    File file = SD.open(filePath.c_str(), FILE_READ);
    while (file.available()) {
      String line = file.readStringUntil('\n');
      // Remove trailing \r if present
      if (line.endsWith("\r")) {
        line = line.substring(0, line.length() - 1);
      }
      lines.push_back(line);
    }
    file.close();
  }

  // If empty or new file, add one blank line
  if (lines.size() == 0) {
    lines.push_back("");
  }

  int currentLine = 0;
  int scrollOffset = 0;
  String inputBuffer = lines[currentLine];
  int cursorPos = inputBuffer.length();
  bool editMode = true;

  auto redraw = [&]() {
    canvas.fillScreen(TFT_BLACK);

    // Draw title bar
    canvas.setTextColor(TFT_WHITE);
    String title = "Edit: " + filename;
    if (title.length() > 38) {
      title = title.substring(0, 35) + "...";
    }
    canvas.drawString(title.c_str(), 2, 0);

    // Draw lines
    canvas.setTextColor(TFT_YELLOW);
    int maxVisibleLines = 12;
    int startLine = max(0, currentLine - maxVisibleLines / 2);
    int endLine = min((int)lines.size(), startLine + maxVisibleLines);

    int yPos = 10;
    for (int i = startLine; i < endLine; i++) {
      if (i == currentLine) {
        // Current line - show with cursor
        canvas.setTextColor(TFT_WHITE);
        String lineNum = String(i + 1) + ":";
        canvas.drawString(lineNum.c_str(), 2, yPos);

        // Draw input buffer
        canvas.setTextColor(TFT_YELLOW);
        canvas.drawString(inputBuffer.c_str(), 20, yPos);

        // Draw cursor
        int cursorX = 20 + cursorPos * 6;
        if (millis() % 1000 < 500) {
          canvas.fillRect(cursorX, yPos, 6, 8, TFT_YELLOW);
        }
      } else {
        // Other lines - show normally
        canvas.setTextColor(TFT_DARKGREY);
        String lineNum = String(i + 1) + ":";
        canvas.drawString(lineNum.c_str(), 2, yPos);

        canvas.setTextColor(TFT_DARKGREY);
        String displayLine = lines[i];
        if (displayLine.length() > 35) {
          displayLine = displayLine.substring(0, 32) + "...";
        }
        canvas.drawString(displayLine.c_str(), 20, yPos);
      }
      yPos += 10;
    }

    // Draw help at bottom
    canvas.fillRect(0, 125, 240, 10, TFT_BLACK);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("` exit | Enter newline | Opt+S save", 2, 127);
  };

  // Edit loop
  while (editMode) {
    redraw();

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

      // Check for backtick to exit
      if (status.word.size() > 0 && status.word[0] == '`') {
        editMode = false;
        break;
      }

      // Check for Option+S to save
      if (status.opt && status.word.size() > 0 && status.word[0] == 's') {
        // Save file
        File file = SD.open(filePath.c_str(), FILE_WRITE);
        if (file) {
          // Save current input buffer to current line
          lines[currentLine] = inputBuffer;

          // Write all lines
          for (int i = 0; i < lines.size(); i++) {
            file.print(lines[i]);
            if (i < lines.size() - 1) {
              file.print("\n");
            }
          }
          file.close();

          // Show save confirmation
          canvas.fillRect(0, 125, 240, 10, TFT_BLACK);
          canvas.setTextColor(TFT_GREEN);
          canvas.drawString("Saved!", 100, 127);
          delay(500);
        }
        continue;
      }

      if (status.enter) {
        // Save current line and move to next
        lines[currentLine] = inputBuffer;
        currentLine++;

        // Insert new line if at end
        if (currentLine >= lines.size()) {
          lines.push_back("");
        }

        inputBuffer = lines[currentLine];
        cursorPos = inputBuffer.length();
      } else if (status.del) {
        // Backspace
        if (cursorPos > 0) {
          inputBuffer = inputBuffer.substring(0, cursorPos - 1) + inputBuffer.substring(cursorPos);
          cursorPos--;
        } else if (currentLine > 0) {
          // Join with previous line
          String prevLine = lines[currentLine - 1];
          lines.erase(lines.begin() + currentLine);
          currentLine--;
          inputBuffer = prevLine + inputBuffer;
          cursorPos = prevLine.length();
          lines[currentLine] = inputBuffer;
        }
      } else {
        // Regular character input
        for (auto key : status.word) {
          if (key >= 32 && key <= 126 && inputBuffer.length() < 60) {
            inputBuffer = inputBuffer.substring(0, cursorPos) + String((char)key) + inputBuffer.substring(cursorPos);
            cursorPos++;
          }
        }
      }
    }

    delay(10);
  }

  // Save current line before exiting
  lines[currentLine] = inputBuffer;

  // Save file
  File file = SD.open(filePath.c_str(), FILE_WRITE);
  if (file) {
    for (int i = 0; i < lines.size(); i++) {
      file.print(lines[i]);
      if (i < lines.size() - 1) {
        file.print("\n");
      }
    }
    file.close();
  }

  // Return to terminal
  addOutput("Saved: " + filePath);
  drawTerminal();
}

void cmd_scan(const String& args) {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    addOutput("Use wifi-connect first");
    return;
  }

  // Parse arguments
  String trimmedArgs = args;
  trimmedArgs.trim();

  // Help flag
  if (trimmedArgs == "-h" || trimmedArgs == "--help") {
    addOutput("=== Network Scanner ===");
    addOutput("Usage: scan [options] <target>");
    addOutput("");
    addOutput("Options:");
    addOutput("  -h, --help    Show help");
    addOutput("  -p <ports>    Port range");
    addOutput("                (default: common)");
    addOutput("  -A            Aggressive scan");
    addOutput("                (more ports + info)");
    addOutput("");
    addOutput("Examples:");
    addOutput("  scan 192.168.1.1");
    addOutput("  scan -p 1-100 192.168.1.1");
    addOutput("  scan -A 192.168.1.0/24");
    addOutput("");
    addOutput("Common ports: 21,22,23,80,");
    addOutput("  443,445,3389,8080,8443");
    return;
  }

  // Parse flags
  bool aggressive = false;
  String portRange = "common";
  String target = "";

  int i = 0;
  while (i < trimmedArgs.length()) {
    // Skip whitespace
    while (i < trimmedArgs.length() && trimmedArgs[i] == ' ') i++;

    if (trimmedArgs.substring(i).startsWith("-A")) {
      aggressive = true;
      i += 2;
    } else if (trimmedArgs.substring(i).startsWith("-p ")) {
      i += 3;
      int spacePos = trimmedArgs.indexOf(' ', i);
      if (spacePos > i) {
        portRange = trimmedArgs.substring(i, spacePos);
        i = spacePos;
      } else {
        portRange = trimmedArgs.substring(i);
        break;
      }
    } else {
      // Rest is target
      target = trimmedArgs.substring(i);
      target.trim();
      break;
    }
  }

  if (target.length() == 0) {
    addOutput("ERROR: No target specified");
    addOutput("Usage: scan <target>");
    addOutput("Try 'scan -h' for help");
    return;
  }

  // Define ports to scan
  std::vector<int> ports;
  if (portRange == "common" || aggressive) {
    // Common ports
    int commonPorts[] = {21, 22, 23, 25, 53, 80, 110, 143, 443, 445, 3306, 3389, 5432, 8080, 8443};
    for (int p : commonPorts) {
      ports.push_back(p);
    }
    if (aggressive) {
      // Add more ports for aggressive scan
      int aggressivePorts[] = {20, 135, 139, 161, 389, 636, 1433, 1521, 2049, 5900, 6379, 27017};
      for (int p : aggressivePorts) {
        ports.push_back(p);
      }
    }
  } else {
    // Parse port range (e.g., "1-100" or "80,443")
    if (portRange.indexOf('-') > 0) {
      int dashPos = portRange.indexOf('-');
      int startPort = portRange.substring(0, dashPos).toInt();
      int endPort = portRange.substring(dashPos + 1).toInt();
      if (startPort > 0 && endPort > startPort && endPort <= 65535) {
        for (int p = startPort; p <= endPort && p <= startPort + 50; p++) {
          ports.push_back(p);
        }
        if (endPort > startPort + 50) {
          addOutput("Note: Limited to 50 ports");
        }
      }
    } else if (portRange.indexOf(',') > 0) {
      // Comma-separated list
      int start = 0;
      while (start < portRange.length() && ports.size() < 50) {
        int commaPos = portRange.indexOf(',', start);
        String portStr = (commaPos > start) ? portRange.substring(start, commaPos) : portRange.substring(start);
        int port = portStr.toInt();
        if (port > 0 && port <= 65535) {
          ports.push_back(port);
        }
        if (commaPos < 0) break;
        start = commaPos + 1;
      }
    } else {
      // Single port
      int port = portRange.toInt();
      if (port > 0 && port <= 65535) {
        ports.push_back(port);
      }
    }
  }

  if (ports.size() == 0) {
    addOutput("ERROR: Invalid port range");
    return;
  }

  addOutput("=== Network Scan ===");
  addOutput("Target: " + target);
  addOutput("Ports:  " + String(ports.size()));
  addOutput("Scanning...");
  addOutput("");
  drawTerminal();

  int openCount = 0;
  int closedCount = 0;

  for (int port : ports) {
    WiFiClient client;
    client.setTimeout(200); // 200ms timeout per port

    if (client.connect(target.c_str(), port)) {
      // Port is open
      String service = "";
      if (port == 21) service = "FTP";
      else if (port == 22) service = "SSH";
      else if (port == 23) service = "Telnet";
      else if (port == 25) service = "SMTP";
      else if (port == 53) service = "DNS";
      else if (port == 80) service = "HTTP";
      else if (port == 110) service = "POP3";
      else if (port == 143) service = "IMAP";
      else if (port == 443) service = "HTTPS";
      else if (port == 445) service = "SMB";
      else if (port == 3306) service = "MySQL";
      else if (port == 3389) service = "RDP";
      else if (port == 5432) service = "PostgreSQL";
      else if (port == 8080) service = "HTTP-Alt";
      else if (port == 8443) service = "HTTPS-Alt";

      String result = String(port) + "/tcp OPEN";
      if (service.length() > 0) {
        result += " (" + service + ")";
      }
      addOutput(result);
      openCount++;
      client.stop();
    } else {
      closedCount++;
    }

    // Update display every few ports
    if ((openCount + closedCount) % 5 == 0) {
      drawTerminal();
    }
  }

  addOutput("");
  addOutput("Scan complete:");
  addOutput("  Open:   " + String(openCount));
  addOutput("  Closed: " + String(closedCount));
}

void cmd_ping(const String& args) {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    addOutput("Use wifi-connect first");
    return;
  }

  String target = args;
  target.trim();

  if (target.length() == 0 || target == "-h" || target == "--help") {
    addOutput("=== Ping ===");
    addOutput("Usage: ping <host>");
    addOutput("");
    addOutput("Examples:");
    addOutput("  ping 192.168.1.1");
    addOutput("  ping google.com");
    addOutput("");
    addOutput("Note: TCP ping on port 80");
    addOutput("(ICMP not available)");
    return;
  }

  addOutput("=== Ping ===");
  addOutput("Target: " + target);
  addOutput("Port: 80 (TCP)");
  addOutput("");
  addOutput("Pinging...");
  drawTerminal();

  int successCount = 0;
  int failCount = 0;
  unsigned long minTime = 999999;
  unsigned long maxTime = 0;
  unsigned long totalTime = 0;

  for (int i = 0; i < 4; i++) {
    WiFiClient client;
    unsigned long startTime = millis();

    if (client.connect(target.c_str(), 80, 2000)) {
      unsigned long responseTime = millis() - startTime;
      addOutput("Reply from " + target + ": time=" + String(responseTime) + "ms");
      successCount++;
      totalTime += responseTime;
      if (responseTime < minTime) minTime = responseTime;
      if (responseTime > maxTime) maxTime = responseTime;
      client.stop();
    } else {
      addOutput("Request timeout");
      failCount++;
    }

    drawTerminal();

    if (i < 3) {
      delay(1000); // Wait 1 second between pings
    }
  }

  addOutput("");
  addOutput("=== Statistics ===");
  addOutput("Sent: 4, Received: " + String(successCount));
  if (successCount > 0) {
    addOutput("Min: " + String(minTime) + "ms");
    addOutput("Max: " + String(maxTime) + "ms");
    addOutput("Avg: " + String(totalTime / successCount) + "ms");
  }
  addOutput("Loss: " + String((failCount * 100) / 4) + "%");
}

void cmd_arp(const String& args) {
  // ARP scan - find all devices on local network
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    return;
  }

  if (args == "-h" || args == "--help") {
    addOutput("=== ARP Table ===");
    addOutput("Usage: arp");
    addOutput("");
    addOutput("Shows ARP cache - devices");
    addOutput("your M5 has communicated with");
    addOutput("on the local network.");
    addOutput("");
    addOutput("Shows: IP and MAC address");
    return;
  }

  addOutput("=== ARP Table ===");
  addOutput("");

  // Show gateway first
  IPAddress gateway = WiFi.gatewayIP();
  uint8_t gatewayMAC[6];
  if (esp_wifi_get_mac(WIFI_IF_STA, gatewayMAC) == ESP_OK) {
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            gatewayMAC[0], gatewayMAC[1], gatewayMAC[2],
            gatewayMAC[3], gatewayMAC[4], gatewayMAC[5]);
    addOutput("Gateway: " + gateway.toString());
    addOutput("  MAC: " + String(macStr));
    addOutput("");
  }

  // Scan local subnet to populate ARP cache
  addOutput("Pinging local network...");
  drawTerminal();

  IPAddress localIP = WiFi.localIP();
  IPAddress subnet = WiFi.subnetMask();

  // Calculate network range
  IPAddress networkAddr;
  for (int i = 0; i < 4; i++) {
    networkAddr[i] = localIP[i] & subnet[i];
  }

  int devicesFound = 0;

  // Quick ping scan to populate ARP cache
  for (int i = 1; i < 255; i++) {
    IPAddress testIP = networkAddr;
    testIP[3] = i;

    // Skip our own IP
    if (testIP == localIP) continue;

    WiFiClient client;
    if (client.connect(testIP, 80, 100)) {  // 100ms timeout
      char ipStr[16];
      sprintf(ipStr, "%d.%d.%d.%d", testIP[0], testIP[1], testIP[2], testIP[3]);
      addOutput(String(ipStr) + " - REACHABLE");
      devicesFound++;
      client.stop();
    }

    // Update display every 25 IPs
    if (i % 25 == 0) {
      drawTerminal();
    }
  }

  addOutput("");
  addOutput("Scan complete.");
  addOutput("Found " + String(devicesFound) + " devices");
  addOutput("");
  addOutput("Note: ARP cache access");
  addOutput("limited on ESP32.");
  addOutput("Shows devices responding");
  addOutput("on port 80 (HTTP).");
}

void cmd_trace(const String& args) {
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    return;
  }

  String target = args;
  target.trim();

  if (target.length() == 0 || target == "-h" || target == "--help") {
    addOutput("=== Traceroute ===");
    addOutput("Usage: trace <host>");
    addOutput("");
    addOutput("Traces network path to host");
    addOutput("using TCP connections with");
    addOutput("increasing TTL values.");
    addOutput("");
    addOutput("Example: trace google.com");
    return;
  }

  addOutput("=== Traceroute ===");
  addOutput("Target: " + target);
  addOutput("");
  addOutput("Note: Limited traceroute");
  addOutput("ESP32 doesn't support TTL");
  addOutput("manipulation easily.");
  addOutput("");
  addOutput("Attempting direct connect...");
  drawTerminal();

  WiFiClient client;
  unsigned long startTime = millis();

  if (client.connect(target.c_str(), 80, 5000)) {
    unsigned long responseTime = millis() - startTime;
    addOutput("1. " + target + " " + String(responseTime) + "ms");
    addOutput("");
    addOutput("Direct connection successful");
    client.stop();
  } else {
    addOutput("Connection failed");
  }
}

void cmd_whois(const String& args) {
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    return;
  }

  String domain = args;
  domain.trim();

  if (domain.length() == 0 || domain == "-h" || domain == "--help") {
    addOutput("=== WHOIS Lookup ===");
    addOutput("Usage: whois <domain>");
    addOutput("");
    addOutput("Queries domain registration");
    addOutput("information.");
    addOutput("");
    addOutput("Example: whois google.com");
    return;
  }

  addOutput("=== WHOIS ===");
  addOutput("Domain: " + domain);
  addOutput("");
  addOutput("Connecting to whois server...");
  drawTerminal();

  WiFiClient client;
  if (client.connect("whois.verisign-grs.com", 43, 5000)) {
    client.println(domain);

    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() > 0 && !line.startsWith(">>>")) {
          addOutput(line);
          drawTerminal();
        }
        timeout = millis();
      }
      delay(10);
    }
    client.stop();
  } else {
    addOutput("ERROR: Cannot connect");
    addOutput("to WHOIS server");
  }
}

void cmd_dns(const String& args) {
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    return;
  }

  String hostname = args;
  hostname.trim();

  if (hostname.length() == 0 || hostname == "-h" || hostname == "--help") {
    addOutput("=== DNS Lookup ===");
    addOutput("Usage: dns <hostname>");
    addOutput("");
    addOutput("Resolves hostname to IP");
    addOutput("address using DNS.");
    addOutput("");
    addOutput("Example: dns google.com");
    return;
  }

  addOutput("=== DNS Lookup ===");
  addOutput("Host: " + hostname);
  addOutput("");
  addOutput("Resolving...");
  drawTerminal();

  IPAddress resolvedIP;
  if (WiFi.hostByName(hostname.c_str(), resolvedIP)) {
    addOutput("IP: " + resolvedIP.toString());

    // Try reverse lookup (PTR)
    addOutput("");
    addOutput("Reverse lookup...");
    drawTerminal();

    // Note: ESP32 doesn't have built-in reverse DNS
    addOutput("(PTR records not supported)");
  } else {
    addOutput("ERROR: Resolution failed");
    addOutput("Host not found");
  }
}

void cmd_netstat(const String& args) {
  if (args == "-h" || args == "--help") {
    addOutput("=== Network Status ===");
    addOutput("Usage: netstat");
    addOutput("");
    addOutput("Shows current network");
    addOutput("connections and statistics.");
    return;
  }

  addOutput("=== Network Status ===");
  addOutput("");

  if (WiFi.status() == WL_CONNECTED) {
    addOutput("WiFi: Connected");
    addOutput("SSID: " + WiFi.SSID());
    addOutput("IP:   " + WiFi.localIP().toString());
    addOutput("Mask: " + WiFi.subnetMask().toString());
    addOutput("Gate: " + WiFi.gatewayIP().toString());
    addOutput("DNS:  " + WiFi.dnsIP().toString());
    addOutput("MAC:  " + WiFi.macAddress());
    addOutput("RSSI: " + String(WiFi.RSSI()) + " dBm");
    addOutput("");
    addOutput("Channel: " + String(WiFi.channel()));

    // Show some stats
    int32_t rssi = WiFi.RSSI();
    String quality = "Excellent";
    if (rssi < -80) quality = "Poor";
    else if (rssi < -70) quality = "Fair";
    else if (rssi < -60) quality = "Good";

    addOutput("Quality: " + quality);
  } else {
    addOutput("WiFi: Disconnected");
  }
}

void cmd_mac(const String& args) {
  String trimmedArgs = args;
  trimmedArgs.trim();
  trimmedArgs.toLowerCase();

  if (trimmedArgs == "-h" || trimmedArgs == "--help") {
    addOutput("=== MAC Changer ===");
    addOutput("Usage:");
    addOutput("  mac show   - Show current");
    addOutput("  mac random - Randomize MAC");
    addOutput("  mac reset  - Reset to orig");
    addOutput("  mac XX:XX:XX:XX:XX:XX");
    addOutput("             - Set custom");
    addOutput("");
    addOutput("Note: Changes take effect");
    addOutput("after WiFi reconnect.");
    return;
  }

  if (trimmedArgs == "show" || trimmedArgs.length() == 0) {
    addOutput("=== MAC Address ===");
    addOutput("Current: " + WiFi.macAddress());
    addOutput("");
    addOutput("Use 'mac random' to change");
    addOutput("Use 'mac reset' to restore");
    return;
  }

  if (trimmedArgs == "random") {
    // Generate random MAC with local bit set
    uint8_t newMac[6];
    newMac[0] = 0x02;  // Locally administered
    for (int i = 1; i < 6; i++) {
      newMac[i] = random(0, 256);
    }

    // Disconnect WiFi first
    if (WiFi.status() == WL_CONNECTED) {
      addOutput("Disconnecting WiFi...");
      WiFi.disconnect();
      delay(100);
    }

    esp_wifi_set_mac(WIFI_IF_STA, newMac);

    addOutput("=== MAC Changed ===");
    addOutput("New: " + WiFi.macAddress());
    addOutput("");
    addOutput("Reconnect to apply changes");
    return;
  }

  if (trimmedArgs == "reset") {
    addOutput("=== MAC Reset ===");
    addOutput("");
    addOutput("To reset MAC, reboot device");
    addOutput("Factory MAC restored on boot");
    return;
  }

  // Custom MAC address
  if (trimmedArgs.length() == 17 && trimmedArgs.indexOf(':') > 0) {
    uint8_t newMac[6];
    int bytesParsed = 0;

    for (int i = 0; i < 17 && bytesParsed < 6; i += 3) {
      String byteStr = trimmedArgs.substring(i, i + 2);
      newMac[bytesParsed] = strtol(byteStr.c_str(), NULL, 16);
      bytesParsed++;
    }

    if (bytesParsed == 6) {
      if (WiFi.status() == WL_CONNECTED) {
        addOutput("Disconnecting WiFi...");
        WiFi.disconnect();
        delay(100);
      }

      esp_wifi_set_mac(WIFI_IF_STA, newMac);

      addOutput("=== MAC Changed ===");
      addOutput("New: " + WiFi.macAddress());
      addOutput("");
      addOutput("Reconnect to apply changes");
    } else {
      addOutput("ERROR: Invalid MAC format");
      addOutput("Use: XX:XX:XX:XX:XX:XX");
    }
    return;
  }

  addOutput("ERROR: Unknown command");
  addOutput("Try 'mac -h' for help");
}

void cmd_speed(const String& args) {
  if (WiFi.status() != WL_CONNECTED) {
    addOutput("ERROR: Not connected to WiFi");
    return;
  }

  if (args == "-h" || args == "--help") {
    addOutput("=== Speed Test ===");
    addOutput("Usage: speed");
    addOutput("");
    addOutput("Tests network speed by");
    addOutput("downloading test data.");
    addOutput("");
    addOutput("Note: Basic speed test");
    addOutput("Results may vary.");
    return;
  }

  addOutput("=== Speed Test ===");
  addOutput("");
  addOutput("Testing download speed...");
  drawTerminal();

  // Test download speed using a small file from a fast server
  WiFiClient client;

  String testHost = "www.google.com";
  int testPort = 80;
  String testPath = "/";

  addOutput("Connecting to " + testHost + "...");
  drawTerminal();

  unsigned long startTime = millis();
  if (client.connect(testHost.c_str(), testPort, 5000)) {
    // Send HTTP request
    client.print("GET " + testPath + " HTTP/1.1\r\n");
    client.print("Host: " + testHost + "\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for response
    unsigned long timeout = millis();
    while (!client.available() && millis() - timeout < 5000) {
      delay(10);
    }

    // Measure download
    int bytesReceived = 0;
    startTime = millis();

    while (client.connected() && millis() - startTime < 3000) {
      if (client.available()) {
        client.read();
        bytesReceived++;
      }
    }

    unsigned long endTime = millis();
    unsigned long duration = endTime - startTime;

    client.stop();

    if (duration > 0 && bytesReceived > 0) {
      float seconds = duration / 1000.0;
      float kbps = (bytesReceived * 8.0) / (seconds * 1024.0);
      float mbps = kbps / 1024.0;

      addOutput("");
      addOutput("=== Results ===");
      addOutput("Downloaded: " + String(bytesReceived) + " bytes");
      addOutput("Time: " + String(duration) + " ms");
      addOutput("");
      if (mbps >= 1.0) {
        addOutput("Speed: " + String(mbps, 2) + " Mbps");
      } else {
        addOutput("Speed: " + String(kbps, 2) + " Kbps");
      }
      addOutput("");
      addOutput("Note: Basic test only");
      addOutput("Results may not reflect");
      addOutput("true connection speed");
    } else {
      addOutput("ERROR: Test failed");
    }
  } else {
    addOutput("ERROR: Cannot connect");
  }
}

void cmd_ip() {
  if (WiFi.status() == WL_CONNECTED) {
    addOutput(WiFi.localIP().toString());
  } else {
    addOutput("Not connected");
  }
  drawTerminal();
}
