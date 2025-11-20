#include "roadmap.h"
#include "config.h"
#include "captive_portal.h"
#include "ui.h"
#include <M5Cardputer.h>

// About screen scroll state
int aboutScrollOffset = 0;
const int LINE_HEIGHT = 18;
unsigned long lastAutoScrollTime = 0;
const int AUTO_SCROLL_INTERVAL = 2000; // 2 seconds between auto-scrolls
bool autoScrollEnabled = true;

// Roadmap item structure
struct RoadmapItem {
  const char* text;
  bool completed;
};

// Roadmap checklist - mark completed items with true
RoadmapItem roadmapItems[] = {
  // CORE SYSTEM
  {"CORE SYSTEM", false},
  {"[X] Boot animation", true},
  {"[X] Star rain landing page", true},
  {"[X] Status bar (WiFi/Time/Battery)", true},
  {"[X] Navigation with GIF animations", true},
  {"[X] Settings (Sound/Brightness/Timezone)", true},

  // FILE MANAGER
  {"", false},
  {"FILE MANAGER", false},
  {"[X] Browse SD card folders", true},
  {"[X] View text files", true},
  {"[X] View images (GIF)", true},
  {"[X] Play audio files (MP3/WAV)", true},
  {"[X] Delete files", true},
  {"[X] Rename files", true},
  {"[X] Copy/Move files", true},
  {"[X] Search files", true},
  {"[X] Batch operations", true},

  // WI-FI
  {"", false},
  {"WI-FI FEATURES", false},
  {"[X] Scan & connect to networks", true},
  {"[X] Save networks", true},
  {"[X] Fake WiFi AP", true},
  {"[X] Captive portals (portalDECK)", true},
  {"[X] Custom HTML portals", true},
  {"[X] Rick Roll beacon spam", true},
  {"[ ] Deauth attacks", false},
  {"[ ] Packet sniffing", false},
  {"[ ] WPA handshake capture", false},

  // FILE TRANSFER
  {"", false},
  {"FILE TRANSFER", false},
  {"[X] Web interface upload/download", true},
  {"[ ] FTP server", false},
  {"[ ] Bluetooth file transfer", false},

  // TERMINAL
  {"", false},
  {"TERMINAL", false},
  {"[X] Basic shell commands", true},
  {"[X] File system navigation", true},
  {"[X] WiFi commands", true},
  {"[X] Scrollable output", true},
  {"[X] Star rain command", true},
  {"[X] Script execution", true},
  {"[X] Text editor", true},
  {"[ ] Lightweight SSH client", false},
  {"[ ] Custom encrypted protocol", false},
  {"[ ] GPIO control", false},

  // RADIO
  {"", false},
  {"RADIO", false},
  {"[X] Internet radio streaming", true},
  {"[X] Station presets", true},
  {"[X] Background playback", true},
  {"[ ] FM radio", false},
  {"[ ] Sub-GHz transceiver", false},

  // MUSIC
  {"", false},
  {"MUSIC PLAYER", false},
  {"[X] Play local MP3 files", true},
  {"[X] Playlist navigation", true},
  {"[X] Play/pause controls", true},
  {"[X] Progress indicator", true},
  {"[X] Background playback", true},
  {"[ ] Shuffle/repeat modes", false},
  {"[ ] Visualizer", false},

  // FUTURE IDEAS
  {"", false},
  {"FUTURE IDEAS", false},
  {"[ ] BadUSB attacks", false},
  {"[ ] Rubber Ducky scripts", false},
  {"[ ] QR code scanner", false},
  {"[ ] Bitcoin wallet", false},
  {"[ ] Games (Snake, Tetris)", false},
  {"[ ] Weather app", false},
  {"[ ] GPS tracking", false},
  {"[ ] Morse code trainer", false},
};

const int totalRoadmapItems = sizeof(roadmapItems) / sizeof(roadmapItems[0]);

// Star emoji - hardcoded for About UI (from OTA)
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

// Helper: Draw 2x scaled star emoji (from OTA)
static void drawStarEmoji2x(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint16_t color = STAR_ICON[row][col];
            if (color != 0x07E0) { // Skip transparent pixels
                // Draw 2x2 block for each pixel
                canvas.fillRect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}

// Helper: Interpolate between blue and white (from OTA)
static uint16_t aboutGradientColor(int line, int totalLines) {
    float ratio = (float)line / (float)totalLines;
    uint8_t r = (uint8_t)(ratio * 31);
    uint8_t g = (uint8_t)(ratio * 63);
    uint8_t b = 31; // Keep blue channel max
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

void enterRoadmap() {
  aboutScrollOffset = 0;
  autoScrollEnabled = true;
  lastAutoScrollTime = millis();
  drawRoadmap();
}

void drawRoadmap() {
  canvas.fillScreen(TFT_BLACK);

  const char* aboutText[] = {
    "",
    "ABOUT",
    "",
    "James Edward",
    "Fauntleroy II",
    "Born: May 16, 1984",
    "",
    "Singer, songwriter,",
    "producer, and",
    "community builder",
    "from Inglewood, CA",
    "",
    "Known for",
    "collaborations with:",
    "",
    "Bruno Mars,",
    "Justin Timberlake,",
    "Rihanna, Beyonce,",
    "Lady Gaga,",
    "Travis Scott,",
    "Frank Ocean,",
    "Kendrick Lamar,",
    "SZA, Drake,",
    "Vince Staples, BTS,",
    "Mariah Carey,",
    "Justin Bieber,",
    "Jay-Z, Chris Brown,",
    "John Mayer",
    "",
    "ACHIEVEMENTS",
    "",
    "Four-time Grammy",
    "Award winner",
    "",
    "Co-founder of",
    "1500 Sound Academy",
    "(Taiwan, Beijing,",
    "Bangkok)",
    "",
    "Founder of",
    "Laboratory",
    "(Workforce Dev)",
    "",
    "Founder of ALL NEW",
    "(Music Pathways)",
    "",
    "Designed Mickey",
    "Mouse toy & statue",
    "for Disney's 100th",
    "",
    "Designed and coded",
    "this device",
    "",
    "---",
    "",
    "2025 Laboratory.mx",
    "",
    ""
  };
  const int numLines = sizeof(aboutText) / sizeof(aboutText[0]);

  canvas.setTextSize(2);

  int y = 15;
  for (int i = 0; i < numLines; i++) {
    int drawY = y - aboutScrollOffset;
    if (drawY >= 10 && drawY < 125) {
      String line = aboutText[i];

      // Yellow for titles (ABOUT, ACHIEVEMENTS), gradient for rest (OTA style)
      if (line == "ABOUT" || line == "ACHIEVEMENTS") {
        canvas.setTextColor(TFT_YELLOW);
      } else {
        // Apply gradient based on position in the VISIBLE area (10-125 range)
        float visibleRatio = (float)(drawY - 10) / (125.0 - 10.0);
        uint8_t r = (uint8_t)(visibleRatio * 31);
        uint8_t g = (uint8_t)(visibleRatio * 63);
        uint8_t b = 31;
        uint16_t gradColor = ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
        canvas.setTextColor(gradColor);
      }

      canvas.drawString(line, 15, drawY); // Left margin: 15px (OTA style)
    }
    y += LINE_HEIGHT;
  }

  // Draw 2x star emoji on bottom right corner (on top of text)
  drawStarEmoji2x(240 - 32 - 10, 135 - 32 - 10);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void updateCreditsScroll() {
  // Auto-scroll with user interrupt capability
  if (autoScrollEnabled && (millis() - lastAutoScrollTime > AUTO_SCROLL_INTERVAL)) {
    aboutScrollOffset += LINE_HEIGHT;
    const int maxScroll = 950; // Approximate max scroll (larger text)
    if (aboutScrollOffset > maxScroll) {
      aboutScrollOffset = maxScroll;
      autoScrollEnabled = false; // Stop at end
    }
    lastAutoScrollTime = millis();
    drawRoadmap();
  }
}

void scrollRoadmapUp() {
  autoScrollEnabled = false; // User interrupt - disable autoscroll
  aboutScrollOffset -= LINE_HEIGHT;
  if (aboutScrollOffset < 0) aboutScrollOffset = 0;
  drawRoadmap();
}

void scrollRoadmapDown() {
  autoScrollEnabled = false; // User interrupt - disable autoscroll
  aboutScrollOffset += LINE_HEIGHT;
  const int maxScroll = 950;  // Approximate max scroll (larger text)
  if (aboutScrollOffset > maxScroll) aboutScrollOffset = maxScroll;
  drawRoadmap();
}
