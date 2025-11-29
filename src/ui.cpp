#include "ui.h"
#include "navigation.h"
#include "background_services.h"
#include "settings.h"
#include <WiFi.h>

// Global canvas for double-buffered rendering (240x135)
M5Canvas canvas(&M5Cardputer.Display);

// Menu animation state
MenuAnimation menuAnim = {false, 0, 0, 0, 0, false};

// Easing function: smooth overshoot (Back ease-out)
// Single continuous curve - overshoots to ~1.12 around t=0.7, settles to 1.0
float easeOutBounce(float t) {
    // Classic "back" ease-out formula
    // Higher overshoot value = more pronounced effect (try 1.7 to 3.0)
    const float overshoot = 0.5f;
    float t1 = t - 1.0f;
    return 1.0f + t1 * t1 * ((overshoot + 1.0f) * t1 + overshoot);
}

// Start a menu animation
void startMenuAnimation(int fromIdx, int toIdx, int dir, bool isApps) {
    menuAnim.active = true;
    menuAnim.startTime = millis();
    menuAnim.fromIndex = fromIdx;
    menuAnim.toIndex = toIdx;
    menuAnim.direction = dir;
    menuAnim.isAppsMenu = isApps;
}

// Update menu animation - returns true if still animating
bool updateMenuAnimation() {
    if (!menuAnim.active) return false;

    extern bool uiInverted;
    bool inverted = uiInverted;
    uint16_t bgColor = inverted ? TFT_BLACK : TFT_WHITE;

    unsigned long elapsed = millis() - menuAnim.startTime;
    float t = (float)elapsed / MENU_ANIM_DURATION;

    if (t >= 1.0f) {
        // Animation complete
        menuAnim.active = false;
        drawScreen(inverted);
        return false;
    }

    // Apply bounce easing
    float eased = easeOutBounce(t);

    // Calculate positions
    // Old card slides out OPPOSITE to direction of movement
    // New card slides in FROM the direction of movement
    int slideDistance = SCREEN_WIDTH + 20;  // Full width + padding

    int oldCardX = (int)(eased * slideDistance * (-menuAnim.direction));
    int newCardX = (int)((1.0f - eased) * slideDistance * menuAnim.direction);

    // Get the labels
    const char* oldLabel;
    const char* newLabel;

    if (menuAnim.isAppsMenu) {
        oldLabel = apps[menuAnim.fromIndex].name.c_str();
        newLabel = apps[menuAnim.toIndex].name.c_str();
    } else {
        oldLabel = mainItems[menuAnim.fromIndex].name.c_str();
        newLabel = mainItems[menuAnim.toIndex].name.c_str();
    }

    // Clear and redraw
    canvas.fillRect(0, 0, 240, 28, bgColor);  // Status bar area
    canvas.fillRect(0, 28, 240, 20, bgColor);  // Dots area
    canvas.fillRect(0, 48, 240, 60, bgColor);  // Card area
    canvas.fillRect(0, 108, 240, 27, bgColor); // Bottom area

    drawStatusBar(inverted);

    // Draw indicator dots for the TARGET index (so they update immediately)
    int totalItems = menuAnim.isAppsMenu ? totalApps : totalMainItems;
    drawIndicatorDots(menuAnim.toIndex, totalItems, inverted);

    // Draw both cards with offsets
    drawCard(oldLabel, oldCardX, inverted);
    drawCard(newLabel, newCardX, inverted);

    // Draw still star (centered, doesn't animate with cards)
    extern void drawStillStar(bool pushToDisplay);
    drawStillStar(false);

    // Draw device name if on main menu
    if (!menuAnim.isAppsMenu) {
        extern SystemSettings settings;
        uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;
        canvas.setTextSize(1);
        canvas.setTextColor(fgColor);
        String deviceName = settings.deviceName;
        if (deviceName.length() > 20) {
            deviceName = deviceName.substring(0, 20);
        }
        int textWidth = deviceName.length() * 6;
        int xPos = 220 - textWidth;
        canvas.drawString(deviceName.c_str(), xPos, 114);
    }

    canvas.pushSprite(0, 0);
    return true;
}

// Berry emoji - hardcoded from cberry.emoji (centered for title bar/WiFi icons)
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

// Draw a simple emoji icon at position with optional scaling
void drawEmojiIcon(int x, int y, const char* emojiBytes, uint16_t color, int scale = 1) {
  // Match specific emojis and draw custom icons
  // Strawberry: F0 9F 8D 93 - replaced with BERRY_ICON
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\x93", 4) == 0) {
    // Draw custom berry emoji from hardcoded array
    for (int py = 0; py < 16; py++) {
      for (int px = 0; px < 16; px++) {
        uint16_t pixelColor = BERRY_ICON[py][px];
        // Skip transparent pixels (lime green = 0x07E0)
        if (pixelColor != 0x07E0) {
          // Draw pixel with scaling
          if (scale == 1) {
            canvas.drawPixel(x + px, y + py, pixelColor);
          } else {
            // For scaled drawing, draw a filled rect for each pixel
            canvas.fillRect(x + px * scale, y + py * scale, scale, scale, pixelColor);
          }
        }
      }
    }
    return;
  }
  // Avocado: F0 9F A5 91
  if (memcmp(emojiBytes, "\xF0\x9F\xA5\x91", 4) == 0) {
    // Draw avocado (green oval with brown center)
    int radius = 3 * scale;
    int centerRadius = 1 * scale;
    int offsetX = 4 * scale;
    int offsetY = 4 * scale;

    canvas.fillCircle(x + offsetX, y + offsetY, radius, TFT_GREEN);
    canvas.fillCircle(x + offsetX, y + offsetY, centerRadius, TFT_BROWN);
    return;
  }

  // Smiling Face 😊: F0 9F 98 8A
  if (memcmp(emojiBytes, "\xF0\x9F\x98\x8A", 4) == 0) {
    int faceX = x + 4 * scale;
    int faceY = y + 4 * scale;
    int faceRadius = 3 * scale;
    canvas.fillCircle(faceX, faceY, faceRadius, TFT_YELLOW);
    // Eyes
    canvas.fillCircle(faceX - 1 * scale, faceY - 1 * scale, scale, TFT_BLACK);
    canvas.fillCircle(faceX + 1 * scale, faceY - 1 * scale, scale, TFT_BLACK);
    // Smile
    canvas.drawLine(faceX - 1 * scale, faceY + 1 * scale, faceX + 1 * scale, faceY + 1 * scale, TFT_BLACK);
    return;
  }

  // Red Heart ❤️: E2 9D A4 (with optional FE0F variant)
  if (memcmp(emojiBytes, "\xE2\x9D\xA4", 3) == 0) {
    int heartX = x + 4 * scale;
    int heartY = y + 4 * scale;
    // Draw two circles for top of heart
    canvas.fillCircle(heartX - 1 * scale, heartY, 2 * scale, TFT_RED);
    canvas.fillCircle(heartX + 1 * scale, heartY, 2 * scale, TFT_RED);
    // Draw triangle for bottom
    canvas.fillTriangle(
      heartX - 3 * scale, heartY,
      heartX + 3 * scale, heartY,
      heartX, heartY + 4 * scale,
      TFT_RED
    );
    return;
  }

  // Thumbs Up 👍: F0 9F 91 8D
  if (memcmp(emojiBytes, "\xF0\x9F\x91\x8D", 4) == 0) {
    int thumbX = x + 4 * scale;
    int thumbY = y + 5 * scale;
    // Thumb (vertical rectangle)
    canvas.fillRect(thumbX - 1 * scale, thumbY - 3 * scale, 2 * scale, 4 * scale, TFT_ORANGE);
    // Fist (horizontal rectangle)
    canvas.fillRect(thumbX - 2 * scale, thumbY, 4 * scale, 2 * scale, TFT_ORANGE);
    return;
  }

  // Face with Tears of Joy 😂: F0 9F 98 82
  if (memcmp(emojiBytes, "\xF0\x9F\x98\x82", 4) == 0) {
    int faceX = x + 4 * scale;
    int faceY = y + 4 * scale;
    int faceRadius = 3 * scale;
    canvas.fillCircle(faceX, faceY, faceRadius, TFT_YELLOW);
    // Squinting eyes (lines)
    canvas.drawLine(faceX - 2 * scale, faceY - 1 * scale, faceX, faceY - 1 * scale, TFT_BLACK);
    canvas.drawLine(faceX, faceY - 1 * scale, faceX + 2 * scale, faceY - 1 * scale, TFT_BLACK);
    // Wide smile
    canvas.drawCircle(faceX, faceY + 1 * scale, 2 * scale, TFT_BLACK);
    // Tears
    canvas.fillCircle(faceX - 3 * scale, faceY - 1 * scale, scale, TFT_CYAN);
    canvas.fillCircle(faceX + 3 * scale, faceY - 1 * scale, scale, TFT_CYAN);
    return;
  }

  // Fire 🔥: F0 9F 94 A5
  if (memcmp(emojiBytes, "\xF0\x9F\x94\xA5", 4) == 0) {
    int fireX = x + 4 * scale;
    int fireY = y + 6 * scale;
    // Outer flame (orange)
    canvas.fillTriangle(
      fireX, fireY - 5 * scale,
      fireX - 3 * scale, fireY,
      fireX + 3 * scale, fireY,
      TFT_ORANGE
    );
    // Inner flame (yellow)
    canvas.fillTriangle(
      fireX, fireY - 3 * scale,
      fireX - 1 * scale, fireY - 1 * scale,
      fireX + 1 * scale, fireY - 1 * scale,
      TFT_YELLOW
    );
    return;
  }

  // Sparkles ✨: E2 9C A8
  if (memcmp(emojiBytes, "\xE2\x9C\xA8", 3) == 0) {
    int sparkleX = x + 4 * scale;
    int sparkleY = y + 4 * scale;
    // Draw a star shape with lines
    canvas.drawLine(sparkleX, sparkleY - 3 * scale, sparkleX, sparkleY + 3 * scale, TFT_YELLOW);
    canvas.drawLine(sparkleX - 3 * scale, sparkleY, sparkleX + 3 * scale, sparkleY, TFT_YELLOW);
    canvas.drawLine(sparkleX - 2 * scale, sparkleY - 2 * scale, sparkleX + 2 * scale, sparkleY + 2 * scale, TFT_YELLOW);
    canvas.drawLine(sparkleX - 2 * scale, sparkleY + 2 * scale, sparkleX + 2 * scale, sparkleY - 2 * scale, TFT_YELLOW);
    return;
  }

  // Skull 💀: F0 9F 92 80
  if (memcmp(emojiBytes, "\xF0\x9F\x92\x80", 4) == 0) {
    int skullX = x + 4 * scale;
    int skullY = y + 4 * scale;
    // Head
    canvas.fillCircle(skullX, skullY, 3 * scale, TFT_WHITE);
    // Eye sockets
    canvas.fillCircle(skullX - 1 * scale, skullY - 1 * scale, scale, TFT_BLACK);
    canvas.fillCircle(skullX + 1 * scale, skullY - 1 * scale, scale, TFT_BLACK);
    // Nose
    canvas.fillRect(skullX - scale/2, skullY, scale, scale, TFT_BLACK);
    // Jaw
    canvas.fillRect(skullX - 2 * scale, skullY + 2 * scale, 4 * scale, scale, TFT_WHITE);
    return;
  }

  // Eyes 👀: F0 9F 91 80
  if (memcmp(emojiBytes, "\xF0\x9F\x91\x80", 4) == 0) {
    int eyeX = x + 3 * scale;
    int eyeY = y + 4 * scale;
    // Left eye
    canvas.fillCircle(eyeX, eyeY, 2 * scale, TFT_WHITE);
    canvas.fillCircle(eyeX, eyeY, scale, TFT_BLACK);
    // Right eye
    canvas.fillCircle(eyeX + 4 * scale, eyeY, 2 * scale, TFT_WHITE);
    canvas.fillCircle(eyeX + 4 * scale, eyeY, scale, TFT_BLACK);
    return;
  }

  // Pineapple 🍍: F0 9F 8D 8D
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\x8D", 4) == 0) {
    int pineX = x + 4 * scale;
    int pineY = y + 5 * scale;
    // 3-pointed star leaves FIRST (Nova Corps style - draws behind body)
    canvas.fillTriangle(
      pineX, pineY - 5 * scale,           // Top point
      pineX - 2 * scale, pineY - 2 * scale,  // Bottom left
      pineX + 2 * scale, pineY - 2 * scale,  // Bottom right
      TFT_GREEN
    );
    // Body (yellow oval) - draws on top
    canvas.fillCircle(pineX, pineY, 3 * scale, TFT_YELLOW);
    return;
  }

  // Robot 🤖: F0 9F A4 96
  if (memcmp(emojiBytes, "\xF0\x9F\xA4\x96", 4) == 0) {
    int robotX = x + 4 * scale;
    int robotY = y + 4 * scale;
    // Head
    canvas.fillRect(robotX - 2 * scale, robotY - 2 * scale, 4 * scale, 4 * scale, TFT_LIGHTGREY);
    // Eyes
    canvas.fillCircle(robotX - 1 * scale, robotY - 1 * scale, scale, TFT_RED);
    canvas.fillCircle(robotX + 1 * scale, robotY - 1 * scale, scale, TFT_RED);
    // Antenna
    canvas.drawLine(robotX, robotY - 2 * scale, robotX, robotY - 4 * scale, TFT_LIGHTGREY);
    canvas.fillCircle(robotX, robotY - 4 * scale, scale, TFT_RED);
    // Mouth
    canvas.drawLine(robotX - 1 * scale, robotY + 1 * scale, robotX + 1 * scale, robotY + 1 * scale, TFT_BLACK);
    return;
  }

  // Ant 🐜: F0 9F 90 9C
  if (memcmp(emojiBytes, "\xF0\x9F\x90\x9C", 4) == 0) {
    int antX = x + 4 * scale;
    int antY = y + 4 * scale;
    // Body (three circles)
    canvas.fillCircle(antX - 2 * scale, antY, 1 * scale, TFT_MAROON);
    canvas.fillCircle(antX, antY, 2 * scale, TFT_MAROON);
    canvas.fillCircle(antX + 2 * scale, antY, 1 * scale, TFT_MAROON);
    // Legs
    canvas.drawLine(antX - 1 * scale, antY, antX - 2 * scale, antY + 2 * scale, TFT_MAROON);
    canvas.drawLine(antX + 1 * scale, antY, antX + 2 * scale, antY + 2 * scale, TFT_MAROON);
    return;
  }

  // Coffee ☕️: E2 98 95
  if (memcmp(emojiBytes, "\xE2\x98\x95", 3) == 0) {
    int cupX = x + 4 * scale;
    int cupY = y + 5 * scale;
    // Cup body
    canvas.fillRect(cupX - 2 * scale, cupY - 2 * scale, 4 * scale, 4 * scale, TFT_WHITE);
    // Coffee inside
    canvas.fillRect(cupX - 1 * scale, cupY - 1 * scale, 2 * scale, 2 * scale, TFT_BROWN);
    // Handle
    canvas.drawLine(cupX + 2 * scale, cupY, cupX + 3 * scale, cupY, TFT_WHITE);
    // Steam
    canvas.drawLine(cupX - 1 * scale, cupY - 3 * scale, cupX - 1 * scale, cupY - 4 * scale, TFT_LIGHTGREY);
    canvas.drawLine(cupX + 1 * scale, cupY - 3 * scale, cupX + 1 * scale, cupY - 4 * scale, TFT_LIGHTGREY);
    return;
  }

  // Radioactive: E2 AD 90 - trefoil symbol
  if (memcmp(emojiBytes, "\xE2\xAD\x90", 3) == 0) {
    int radX = x + 4 * scale;
    int radY = y + 4 * scale;
    // Center circle
    canvas.fillCircle(radX, radY, scale, TFT_YELLOW);
    // Three blades at 120° angles
    for (int i = 0; i < 3; i++) {
      float angle = (i * 2.0944f) - 1.5708f; // 120° spacing, start at top
      int x1 = radX + (int)(2 * scale * cos(angle));
      int y1 = radY + (int)(2 * scale * sin(angle));
      int x2 = radX + (int)(3 * scale * cos(angle - 0.5f));
      int y2 = radY + (int)(3 * scale * sin(angle - 0.5f));
      int x3 = radX + (int)(3 * scale * cos(angle + 0.5f));
      int y3 = radY + (int)(3 * scale * sin(angle + 0.5f));
      canvas.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    }
    return;
  }

  // Cake 🍰: F0 9F 8D B0
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\xB0", 4) == 0) {
    int cakeX = x + 4 * scale;
    int cakeY = y + 5 * scale;
    // Bottom layer (pink)
    canvas.fillRect(cakeX - 3 * scale, cakeY, 6 * scale, 2 * scale, TFT_PINK);
    // Top layer (pink)
    canvas.fillRect(cakeX - 2 * scale, cakeY - 2 * scale, 4 * scale, 2 * scale, TFT_PINK);
    // Frosting/cherry on top (red)
    canvas.fillCircle(cakeX, cakeY - 2 * scale, scale, TFT_RED);
    return;
  }

  // Pill/Capsule: F0 9F 8D 89 - half red, half white
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\x89", 4) == 0) {
    int pillX = x + 4 * scale;
    int pillY = y + 4 * scale;
    // Capsule outline
    canvas.drawCircle(pillX - 2 * scale, pillY, 2 * scale, TFT_WHITE);
    canvas.drawCircle(pillX + 2 * scale, pillY, 2 * scale, TFT_RED);
    canvas.drawLine(pillX - 2 * scale, pillY - 2 * scale, pillX + 2 * scale, pillY - 2 * scale, TFT_WHITE);
    canvas.drawLine(pillX - 2 * scale, pillY + 2 * scale, pillX + 2 * scale, pillY + 2 * scale, TFT_WHITE);
    // Fill left half white
    canvas.fillCircle(pillX - 2 * scale, pillY, scale, TFT_WHITE);
    canvas.fillRect(pillX - 2 * scale, pillY - scale, 2 * scale, 2 * scale, TFT_WHITE);
    // Fill right half red
    canvas.fillCircle(pillX + 2 * scale, pillY, scale, TFT_RED);
    canvas.fillRect(pillX, pillY - scale, 2 * scale, 2 * scale, TFT_RED);
    return;
  }

  // Eggplant 🍆: F0 9F 8D 86
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\x86", 4) == 0) {
    int eggX = x + 4 * scale;
    int eggY = y + 5 * scale;
    // Purple body
    canvas.fillCircle(eggX, eggY, 3 * scale, TFT_PURPLE);
    canvas.fillRect(eggX - 2 * scale, eggY - 1 * scale, 4 * scale, 2 * scale, TFT_PURPLE);
    // Green stem/leaves
    canvas.fillRect(eggX - 1 * scale, eggY - 4 * scale, 2 * scale, 2 * scale, TFT_GREEN);
    return;
  }

  // Atom: F0 9F 90 9A - nucleus with electron orbits
  if (memcmp(emojiBytes, "\xF0\x9F\x90\x9A", 4) == 0) {
    int atomX = x + 4 * scale;
    int atomY = y + 4 * scale;
    // Nucleus (red)
    canvas.fillCircle(atomX, atomY, scale, TFT_RED);
    // Electron orbits (cyan ellipses)
    canvas.drawCircle(atomX, atomY, 3 * scale, TFT_CYAN);
    canvas.drawEllipse(atomX, atomY, 3 * scale, scale, TFT_CYAN);
    canvas.drawEllipse(atomX, atomY, scale, 3 * scale, TFT_CYAN);
    // Electrons (white dots)
    canvas.fillCircle(atomX + 3 * scale, atomY, scale, TFT_WHITE);
    canvas.fillCircle(atomX, atomY - 3 * scale, scale, TFT_WHITE);
    return;
  }

  // Peppermint 🍬: F0 9F 8D AC - red and white swirl
  if (memcmp(emojiBytes, "\xF0\x9F\x8D\xAC", 4) == 0) {
    int candyX = x + 4 * scale;
    int candyY = y + 4 * scale;
    // White circle base
    canvas.fillCircle(candyX, candyY, 3 * scale, TFT_WHITE);
    // Red swirl stripes
    canvas.drawLine(candyX - 2 * scale, candyY - 2 * scale, candyX + 2 * scale, candyY + 2 * scale, TFT_RED);
    canvas.drawLine(candyX - 2 * scale, candyY + 2 * scale, candyX + 2 * scale, candyY - 2 * scale, TFT_RED);
    canvas.drawCircle(candyX, candyY, 2 * scale, TFT_RED);
    return;
  }

  // Biohazard: F0 9F 94 A5 - three crescents
  if (memcmp(emojiBytes, "\xF0\x9F\x94\xA5", 4) == 0) {
    int bioX = x + 4 * scale;
    int bioY = y + 4 * scale;
    // Center circle
    canvas.fillCircle(bioX, bioY, scale, TFT_ORANGE);
    // Three crescents at 120° angles
    for (int i = 0; i < 3; i++) {
      float angle = (i * 2.0944f); // 120° spacing
      int cx = bioX + (int)(2 * scale * cos(angle));
      int cy = bioY + (int)(2 * scale * sin(angle));
      canvas.fillCircle(cx, cy, 2 * scale, TFT_ORANGE);
      canvas.fillCircle(cx + (int)(scale * cos(angle)), cy + (int)(scale * sin(angle)), scale, TFT_BLACK);
    }
    return;
  }

  // Syringe: F0 9F 92 80 - needle with plunger
  if (memcmp(emojiBytes, "\xF0\x9F\x92\x80", 4) == 0) {
    int syrX = x + 4 * scale;
    int syrY = y + 4 * scale;
    // Barrel (white)
    canvas.fillRect(syrX - 2 * scale, syrY - scale, 4 * scale, 2 * scale, TFT_WHITE);
    canvas.drawRect(syrX - 2 * scale, syrY - scale, 4 * scale, 2 * scale, TFT_DARKGREY);
    // Needle (grey)
    canvas.fillTriangle(syrX + 2 * scale, syrY - scale, syrX + 2 * scale, syrY + scale, syrX + 4 * scale, syrY, TFT_DARKGREY);
    // Plunger (cyan)
    canvas.fillRect(syrX - 3 * scale, syrY - scale/2, scale, scale, TFT_CYAN);
    return;
  }

  // Rocket 🚀: F0 9F 9A 80 - red rocket
  if (memcmp(emojiBytes, "\xF0\x9F\x9A\x80", 4) == 0) {
    int rocketX = x + 4 * scale;
    int rocketY = y + 5 * scale;
    // Body (red)
    canvas.fillRect(rocketX - 1 * scale, rocketY - 2 * scale, 2 * scale, 4 * scale, TFT_RED);
    // Nose cone (red triangle)
    canvas.fillTriangle(rocketX, rocketY - 4 * scale, rocketX - 1 * scale, rocketY - 2 * scale, rocketX + 1 * scale, rocketY - 2 * scale, TFT_RED);
    // Window (white)
    canvas.fillCircle(rocketX, rocketY, scale, TFT_WHITE);
    // Fins
    canvas.fillTriangle(rocketX - 1 * scale, rocketY + 2 * scale, rocketX - 2 * scale, rocketY + 3 * scale, rocketX - 1 * scale, rocketY + 1 * scale, TFT_ORANGE);
    canvas.fillTriangle(rocketX + 1 * scale, rocketY + 2 * scale, rocketX + 2 * scale, rocketY + 3 * scale, rocketX + 1 * scale, rocketY + 1 * scale, TFT_ORANGE);
    return;
  }

  // DNA Helix: E2 9A A1 - double helix spiral
  if (memcmp(emojiBytes, "\xE2\x9A\xA1", 3) == 0) {
    int dnaX = x + 4 * scale;
    int dnaY = y + 4 * scale;
    // Draw helix strands (two sine waves)
    for (int i = -3 * scale; i <= 3 * scale; i++) {
      int offset1 = (int)(scale * sin(i * 0.5f));
      int offset2 = (int)(scale * sin(i * 0.5f + 3.14159f));
      canvas.drawPixel(dnaX + offset1, dnaY + i, TFT_CYAN);
      canvas.drawPixel(dnaX + offset2, dnaY + i, TFT_MAGENTA);
      // Connection rungs every few pixels
      if (i % (2 * scale) == 0) {
        canvas.drawLine(dnaX + offset1, dnaY + i, dnaX + offset2, dnaY + i, TFT_WHITE);
      }
    }
    return;
  }

  // Beaker/Flask: F0 9F 8E B5 - lab glassware
  if (memcmp(emojiBytes, "\xF0\x9F\x8E\xB5", 4) == 0) {
    int beakerX = x + 4 * scale;
    int beakerY = y + 5 * scale;
    // Flask body (trapezoid)
    canvas.drawLine(beakerX - 2 * scale, beakerY - 2 * scale, beakerX - 3 * scale, beakerY + 2 * scale, TFT_WHITE);
    canvas.drawLine(beakerX + 2 * scale, beakerY - 2 * scale, beakerX + 3 * scale, beakerY + 2 * scale, TFT_WHITE);
    canvas.drawLine(beakerX - 3 * scale, beakerY + 2 * scale, beakerX + 3 * scale, beakerY + 2 * scale, TFT_WHITE);
    // Liquid inside (cyan)
    canvas.fillTriangle(beakerX - 2 * scale, beakerY, beakerX + 2 * scale, beakerY, beakerX, beakerY + 2 * scale, TFT_CYAN);
    // Opening
    canvas.drawLine(beakerX - 2 * scale, beakerY - 2 * scale, beakerX + 2 * scale, beakerY - 2 * scale, TFT_WHITE);
    return;
  }

  // Space Invader 👾: F0 9F 91 BE - purple alien
  if (memcmp(emojiBytes, "\xF0\x9F\x91\xBE", 4) == 0) {
    int alienX = x + 4 * scale;
    int alienY = y + 4 * scale;
    // Body (purple)
    canvas.fillRect(alienX - 2 * scale, alienY - 1 * scale, 4 * scale, 3 * scale, TFT_PURPLE);
    // Head bumps
    canvas.fillRect(alienX - 3 * scale, alienY, scale, scale, TFT_PURPLE);
    canvas.fillRect(alienX + 2 * scale, alienY, scale, scale, TFT_PURPLE);
    // Eyes (white)
    canvas.fillRect(alienX - 1 * scale, alienY, scale, scale, TFT_WHITE);
    canvas.fillRect(alienX, alienY, scale, scale, TFT_WHITE);
    return;
  }

  // Target/Crosshair: F0 9F 8E AE - concentric circles with cross
  if (memcmp(emojiBytes, "\xF0\x9F\x8E\xAE", 4) == 0) {
    int targetX = x + 4 * scale;
    int targetY = y + 4 * scale;
    // Concentric circles (red)
    canvas.drawCircle(targetX, targetY, 3 * scale, TFT_RED);
    canvas.drawCircle(targetX, targetY, 2 * scale, TFT_RED);
    canvas.drawCircle(targetX, targetY, scale, TFT_RED);
    // Crosshair (white)
    canvas.drawLine(targetX - 4 * scale, targetY, targetX + 4 * scale, targetY, TFT_WHITE);
    canvas.drawLine(targetX, targetY - 4 * scale, targetX, targetY + 4 * scale, TFT_WHITE);
    return;
  }

  // Crescent Moon 🌙: F0 9F 8C 99 - yellow crescent
  if (memcmp(emojiBytes, "\xF0\x9F\x8C\x99", 4) == 0) {
    int moonX = x + 4 * scale;
    int moonY = y + 4 * scale;
    // Outer circle (yellow)
    canvas.fillCircle(moonX, moonY, 3 * scale, TFT_YELLOW);
    // Cut out crescent (works on black background)
    canvas.fillCircle(moonX + 1 * scale, moonY - 1 * scale, 3 * scale, TFT_BLACK);
    return;
  }

  // Heart ❤️: E2 9D A4 - red heart
  if (memcmp(emojiBytes, "\xE2\x9D\xA4", 3) == 0) {
    int heartX = x + 4 * scale;
    int heartY = y + 4 * scale;
    // Two circles for top bumps
    canvas.fillCircle(heartX - 1 * scale, heartY - 1 * scale, 2 * scale, TFT_RED);
    canvas.fillCircle(heartX + 1 * scale, heartY - 1 * scale, 2 * scale, TFT_RED);
    // Bottom triangle
    canvas.fillTriangle(heartX - 3 * scale, heartY, heartX + 3 * scale, heartY, heartX, heartY + 3 * scale, TFT_RED);
    return;
  }

  // Diamond 💎: F0 9F 92 8E - cyan diamond
  if (memcmp(emojiBytes, "\xF0\x9F\x92\x8E", 4) == 0) {
    int gemX = x + 4 * scale;
    int gemY = y + 4 * scale;
    // Top triangle (cyan)
    canvas.fillTriangle(gemX - 2 * scale, gemY, gemX + 2 * scale, gemY, gemX, gemY - 2 * scale, TFT_CYAN);
    // Bottom triangle (cyan)
    canvas.fillTriangle(gemX - 2 * scale, gemY, gemX + 2 * scale, gemY, gemX, gemY + 3 * scale, TFT_CYAN);
    return;
  }

  // Default: draw a small square for unknown emoji
  canvas.fillRect(x + 2 * scale, y + 2 * scale, 5 * scale, 5 * scale, color);
}

// Helper function to render string with emoji icons
String renderWithEmojis(const String& str, int& emojiCount) {
  String result = "";
  emojiCount = 0;

  for (int i = 0; i < str.length(); i++) {
    uint8_t c = (uint8_t)str[i];

    // If it's a regular ASCII character (0-127), keep it
    if (c < 128) {
      result += (char)c;
    } else {
      // We hit a multi-byte UTF-8 sequence (emoji)
      if (c >= 0xF0 && i + 3 < str.length()) {
        // 4-byte emoji - skip it in text, we'll draw it as icon
        emojiCount++;
        result += "  "; // Leave space for icon (2 chars worth)
        i += 3;
      } else if (c >= 0xE0 && i + 2 < str.length()) {
        // 3-byte sequence
        emojiCount++;
        result += "  ";
        i += 2;
      } else if (c >= 0xC0 && i + 1 < str.length()) {
        // 2-byte sequence
        emojiCount++;
        result += "  ";
        i += 1;
      }
    }
  }

  return result;
}

// Helper function to get UTF-8 character count (not byte count)
int getUTF8Length(const String& str) {
  int count = 0;
  for (int i = 0; i < str.length(); ) {
    uint8_t c = str[i];
    if (c < 0x80) {
      i += 1; // Single-byte character (ASCII)
    } else if (c < 0xE0) {
      i += 2; // 2-byte character
    } else if (c < 0xF0) {
      i += 3; // 3-byte character (most emojis)
    } else {
      i += 4; // 4-byte character
    }
    count++;
  }
  return count;
}

// Helper function to truncate UTF-8 string by character count
String truncateUTF8(const String& str, int maxChars) {
  int count = 0;
  int bytePos = 0;

  for (int i = 0; i < str.length() && count < maxChars; ) {
    uint8_t c = str[i];
    int charBytes = 1;

    if (c < 0x80) {
      charBytes = 1;
    } else if (c < 0xE0) {
      charBytes = 2;
    } else if (c < 0xF0) {
      charBytes = 3;
    } else {
      charBytes = 4;
    }

    i += charBytes;
    bytePos = i;
    count++;
  }

  return str.substring(0, bytePos);
}

// Cached time to avoid blocking getLocalTime() calls on every draw
static String cachedTime = "--:--";
static unsigned long lastTimeUpdate = 0;
const unsigned long TIME_UPDATE_INTERVAL = 30000; // Update every 30 seconds

String getCurrentTime() {
  // Return cached time if recently updated (within 30 seconds)
  if (millis() - lastTimeUpdate < TIME_UPDATE_INTERVAL) {
    return cachedTime;
  }

  // Only update if WiFi is connected (NTP needs network)
  if (WiFi.status() != WL_CONNECTED) {
    cachedTime = "--:--";
    return cachedTime;
  }

  // Try to get time (non-blocking with timeout)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) { // 100ms timeout instead of default 5000ms
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    cachedTime = String(timeStr);
    lastTimeUpdate = millis();
  }

  return cachedTime;
}

// Battery reading smoothing and caching
static int batteryReadings[10] = {0};
static int batteryReadingIndex = 0;
static bool batteryReadingsInitialized = false;
static int cachedBatteryPercent = 0;
static bool cachedChargingState = false;
static unsigned long lastBatteryUpdate = 0;

// Accurate LiPo discharge curve (voltage -> percentage)
int voltageToPercent(int voltage) {
  // LiPo discharge curve lookup table (voltage in mV -> %)
  // Based on typical 18650/LiPo discharge characteristics
  if (voltage >= 4200) return 100;
  if (voltage >= 4100) return 95;
  if (voltage >= 4000) return 85;
  if (voltage >= 3900) return 70;
  if (voltage >= 3800) return 50;
  if (voltage >= 3700) return 30;
  if (voltage >= 3600) return 15;
  if (voltage >= 3500) return 8;
  if (voltage >= 3400) return 3;
  if (voltage >= 3300) return 1;
  return 0;
}

void updateBatteryReadings() {
  int voltage = M5Cardputer.Power.getBatteryVoltage();

  // Initialize smoothing array on first call
  if (!batteryReadingsInitialized) {
    for (int i = 0; i < 10; i++) {
      batteryReadings[i] = voltage;
    }
    batteryReadingsInitialized = true;
  }

  // Add new reading to circular buffer
  batteryReadings[batteryReadingIndex] = voltage;
  batteryReadingIndex = (batteryReadingIndex + 1) % 10;

  // Calculate average voltage
  int avgVoltage = 0;
  for (int i = 0; i < 10; i++) {
    avgVoltage += batteryReadings[i];
  }
  avgVoltage /= 10;

  // Convert to percentage using discharge curve and cache it
  cachedBatteryPercent = voltageToPercent(avgVoltage);

  // Update charging state
  cachedChargingState = M5Cardputer.Power.isCharging();

  lastBatteryUpdate = millis();
}

int getBatteryPercent() {
  // Update battery readings every 10 minutes
  if (millis() - lastBatteryUpdate > 600000 || !batteryReadingsInitialized) {
    updateBatteryReadings();
  }
  return cachedBatteryPercent;
}

bool isCharging() {
  // Return cached charging state (updated with battery readings)
  if (millis() - lastBatteryUpdate > 600000 || !batteryReadingsInitialized) {
    updateBatteryReadings();
  }
  return cachedChargingState;
}

void drawStar(int x, int y, int size, float angle, uint16_t fillColor, uint16_t outlineColor) {
  const int points = 5;
  const float innerRadius = size * 0.38;
  const float outerRadius = size * 0.85;
  
  int xPoints[points * 2];
  int yPoints[points * 2];
  
  for (int i = 0; i < points * 2; i++) {
    float currentAngle = angle + (i * PI / points) - PI/2;
    float radius = (i % 2 == 0) ? outerRadius : innerRadius;
    xPoints[i] = x + radius * cos(currentAngle);
    yPoints[i] = y + radius * sin(currentAngle);
  }
  
  for (int i = 0; i < points; i++) {
    int next = (i + 1) % points;
    canvas.fillTriangle(
      x, y, 
      xPoints[i*2], yPoints[i*2], 
      xPoints[next*2], yPoints[next*2], 
      fillColor
    );
  }
  
  for (int thickness = 0; thickness < 3; thickness++) {
    for (int i = 0; i < points * 2; i++) {
      int next = (i + 1) % (points * 2);
      canvas.drawLine(
        xPoints[i] + thickness, yPoints[i], 
        xPoints[next] + thickness, yPoints[next], 
        outlineColor
      );
      canvas.drawLine(
        xPoints[i], yPoints[i] + thickness, 
        xPoints[next], yPoints[next] + thickness, 
        outlineColor
      );
    }
  }
}

void drawIndicatorDots(int currentIndex, int totalItems, bool inverted) {
  uint16_t activeDotColor = inverted ? TFT_WHITE : TFT_BLACK;
  uint16_t inactiveDotColor = inverted ? TFT_DARKGREY : TFT_LIGHTGREY;
  
  int dotSize = 4;
  int dotSpacing = 10;
  int totalWidth = (totalItems * dotSpacing) - dotSpacing;
  int startX = (240 - totalWidth) / 2;
  int y = 40;
  
  for (int i = 0; i < totalItems; i++) {
    int x = startX + (i * dotSpacing);
    if (i == currentIndex) {
      canvas.fillCircle(x, y, dotSize, activeDotColor);
    } else {
      canvas.fillCircle(x, y, dotSize/2, inactiveDotColor);
    }
  }
}

void drawStatusBar(bool inverted) {
#if DEBUG_ENABLE_STATUSBAR == 0
  // Minimal status bar - just a line
  uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;
  canvas.drawLine(0, 25, 240, 25, fgColor);
  return;
#endif

  uint16_t bgColor = inverted ? TFT_BLACK : TFT_WHITE;
  uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;

  int wifiWidth = 110;
  canvas.fillRoundRect(5, 5, wifiWidth, 18, 9, bgColor);
  for (int i = 0; i < 2; i++) {
    canvas.drawRoundRect(5+i, 5+i, wifiWidth-i*2, 18-i*2, 9-i, fgColor);
  }

#if DEBUG_ENABLE_WIFI
  wifiConnected = (WiFi.status() == WL_CONNECTED);
#else
  wifiConnected = false;
#endif
  if (wifiConnected) {
    String rawSSID = WiFi.SSID();

    // Draw text with emoji icons
    canvas.setTextSize(1);
    uint16_t textColor = (wifiConnected ? (inverted ? TFT_WHITE : TFT_BLACK) : TFT_RED);
    canvas.setTextColor(textColor);

    int textX = 12;
    int textY = 10;

    // Parse and draw string with emojis
    for (int i = 0; i < rawSSID.length() && textX < 110; i++) {
      uint8_t c = (uint8_t)rawSSID[i];

      if (c < 128) {
        // Regular ASCII - draw it
        canvas.drawChar(c, textX, textY);
        textX += 6;
      } else if (c >= 0xF0 && i + 3 < rawSSID.length()) {
        // 4-byte emoji - draw icon
        char emojiBytes[5];
        emojiBytes[0] = rawSSID[i];
        emojiBytes[1] = rawSSID[i+1];
        emojiBytes[2] = rawSSID[i+2];
        emojiBytes[3] = rawSSID[i+3];
        emojiBytes[4] = 0;

        // Debug: print the bytes to serial
        Serial.printf("Emoji bytes: %02X %02X %02X %02X\n",
                     (uint8_t)emojiBytes[0],
                     (uint8_t)emojiBytes[1],
                     (uint8_t)emojiBytes[2],
                     (uint8_t)emojiBytes[3]);

        drawEmojiIcon(textX, 6, emojiBytes, textColor);  // y=6 to center 16px emoji in 18px bar
        textX += 10; // Space for emoji icon
        i += 3;
      } else {
        // Skip unknown multi-byte sequences
        if (c >= 0xE0) i += 2;
        else if (c >= 0xC0) i += 1;
      }
    }

    wifiSSID = rawSSID; // Store for other uses
  } else {
    wifiSSID = "LAB - [Offline]";
    canvas.setTextSize(1);

    // Orange-yellow gradient colors
    uint16_t colors[] = {
      0xFB00,   // Deep orange (L)
      0xFC00,   // Orange (A)
      0xFC80,   // Light orange (B)
      0xFD00,   // Orange-yellow (space)
      0xFD80,   // Bright orange (-)
      0xFE00,   // Yellow-orange (space)
      0xFE40,   // Golden ([)
      0xFD20,   // Amber (O)
      0xFC60,   // Burnt orange (f)
      0xFB80,   // Dark orange (f)
      0xFC00,   // Orange (l)
      0xFD00,   // Orange-yellow (i)
      0xFE00,   // Yellow-orange (n)
      0xFD80,   // Bright orange (e)
      0xFC80    // Light orange (])
    };

    // Draw static text with gradient
    int textX = 12;
    int textY = 10;
    for (int i = 0; i < wifiSSID.length(); i++) {
      canvas.setTextColor(colors[i]);
      canvas.drawChar(wifiSSID[i], textX, textY);
      textX += 6;
    }
  }
  
  canvas.fillRoundRect(120, 5, 55, 18, 9, bgColor);
  for (int i = 0; i < 2; i++) {
    canvas.drawRoundRect(120+i, 5+i, 55-i*2, 18-i*2, 9-i, fgColor);
  }
  canvas.setTextColor(fgColor);
#if DEBUG_ENABLE_TIME
  canvas.drawString(getCurrentTime().c_str(), 132, 10);
#else
  canvas.drawString("--:--", 132, 10);
#endif
  
  // Battery indicator - colored based on charge level
#if DEBUG_ENABLE_BATTERY
  int batteryPercent = getBatteryPercent();

  // Choose color based on battery level
  uint16_t batteryColor;
  if (batteryPercent > 50) {
    batteryColor = TFT_GREEN;
  } else if (batteryPercent > 20) {
    batteryColor = TFT_YELLOW;
  } else {
    batteryColor = TFT_RED;
  }
#else
  uint16_t batteryColor = TFT_DARKGREY;  // Gray when disabled
  int batteryPercent = 0;
#endif

  // Fill the entire battery box with the color
  canvas.fillRoundRect(180, 5, 55, 18, 9, batteryColor);

  // Add outline
  for (int i = 0; i < 2; i++) {
    canvas.drawRoundRect(180+i, 5+i, 55-i*2, 18-i*2, 9-i, fgColor);
  }

#if DEBUG_ENABLE_BATTERY
  // Show percentage text in center of battery
  canvas.setTextSize(1);
  canvas.setTextColor(fgColor);
  String percentText = String(batteryPercent) + "%";
  int textWidth = percentText.length() * 6;
  int textX = 207 - (textWidth / 2);
  canvas.drawString(percentText.c_str(), textX, 9);
#endif

  // Message notification indicator (heart icon over battery)
  extern bool hasUnreadMessages;
  if (hasUnreadMessages) {
    // Draw a small heart icon on right side
    int heartX = 225;
    int heartY = 9;
    canvas.fillCircle(heartX, heartY + 2, 2, TFT_BLUE);
    canvas.fillCircle(heartX + 4, heartY + 2, 2, TFT_BLUE);
    canvas.fillTriangle(
      heartX - 2, heartY + 3,
      heartX + 6, heartY + 3,
      heartX + 2, heartY + 9,
      TFT_BLUE
    );
  }

  // Background service indicators (colored circles between time and battery)
#if DEBUG_ENABLE_BG_SERVICES
  ServiceStatus status = getServiceStatus();
  int iconX = 175;
  int iconY = 12;

  // Show circle icons for active services
  if (status.portalRunning) {
    // Portal - Cyan circle
    canvas.fillCircle(iconX, iconY, 4, TFT_CYAN);
    iconX -= 10;
  }

  if (status.fakeAPRunning) {
    // Fake WiFi - Blue circle
    canvas.fillCircle(iconX, iconY, 4, TFT_BLUE);
    iconX -= 10;
  }

  if (status.transferRunning) {
    // Transfer - Green circle
    canvas.fillCircle(iconX, iconY, 4, TFT_GREEN);
  }
#endif
}

void drawCard(const char* label, int xOffset, bool inverted) {
  int cardX = 20 + xOffset;
  int cardY = 50;
  int cardW = 200;
  int cardH = 50;

  // Skip drawing if card is completely off-screen
  if (cardX + cardW < 0 || cardX > 240) return;

  uint16_t bgColor = inverted ? TFT_BLACK : TFT_WHITE;
  uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;
  uint16_t fillColor = bgColor;  // Default to background color

  // Special colored backgrounds for APPS menu (page 2)
  // Check by label since we might be animating between menus
  if (!inverted) {
    if (strcmp(label, "Files") == 0) {
      fillColor = TFT_YELLOW;  // System yellow
    } else if (strcmp(label, "Fun") == 0) {
      fillColor = 0xAEFD;  // Powder blue
    } else if (strcmp(label, "Transfer") == 0) {
      fillColor = 0xF800;  // Red #FF0000
    } else if (strcmp(label, "Terminal") == 0) {
      fillColor = 0xAFE5;  // Lime green (more yellow - chartreuse)
    } else if (strcmp(label, "Music") == 0) {
      fillColor = 0xC99F;  // Lavender
    }
  }

  canvas.fillRoundRect(cardX, cardY, cardW, cardH, 25, fillColor);
  canvas.drawRoundRect(cardX, cardY, cardW, cardH, 25, fgColor);
  canvas.drawRoundRect(cardX+1, cardY+1, cardW-2, cardH-2, 24, fgColor);
  canvas.drawRoundRect(cardX+2, cardY+2, cardW-4, cardH-4, 23, fgColor);
  canvas.drawRoundRect(cardX+3, cardY+3, cardW-6, cardH-6, 22, fgColor);

  canvas.setTextSize(3);
  canvas.setTextColor(fgColor);
  int textWidth = strlen(label) * 18;
  int textX = cardX + (cardW - textWidth) / 2;
  canvas.drawString(label, textX, cardY + 13);
}

void drawPlaceholderScreen(int screenNum, const char* title, bool inverted) {
  uint16_t bgColor = inverted ? TFT_BLACK : TFT_WHITE;
  uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;
  
  canvas.fillScreen(bgColor);
  drawStatusBar(inverted);
  
  canvas.setTextSize(8);
  canvas.setTextColor(fgColor);
  String numStr = String(screenNum);
  int numWidth = numStr.length() * 48;
  canvas.drawString(numStr.c_str(), (240 - numWidth) / 2, 40);
  
  canvas.setTextSize(2);
  int titleWidth = strlen(title) * 12;
  canvas.drawString(title, (240 - titleWidth) / 2, 95);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press ` to return", 60, 120);

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawScreen(bool inverted) {
  uint16_t bgColor = inverted ? TFT_BLACK : TFT_WHITE;
  uint16_t fgColor = inverted ? TFT_WHITE : TFT_BLACK;

  // Clear regions selectively to avoid flashing
  // Clear top area (status bar)
  canvas.fillRect(0, 0, 240, 28, bgColor);
  // Clear middle area (dots)
  canvas.fillRect(0, 28, 240, 20, bgColor);
  // Clear card area
  canvas.fillRect(0, 48, 240, 60, bgColor);
  // Clear bottom area (includes star and device name)
  canvas.fillRect(0, 108, 240, 27, bgColor);

  drawStatusBar(inverted);

  if (currentState == APPS_MENU) {
    drawCard(apps[currentAppIndex].name.c_str(), 0, inverted);
    drawIndicatorDots(currentAppIndex, totalApps, inverted);
    // Draw static star (don't push yet - more drawing follows)
    drawStillStar(false);
  } else if (currentState == MAIN_MENU) {
    drawCard(mainItems[currentMainIndex].name.c_str(), 0, inverted);
    drawIndicatorDots(currentMainIndex, totalMainItems, inverted);
    // Draw static star (don't push yet - device name drawn after)
    drawStillStar(false);
  }

  // Draw device name AFTER star so it appears on top (star overlaps at y=110-126)
  if (currentState == MAIN_MENU) {
    canvas.setTextSize(1);
    canvas.setTextColor(fgColor);
    String deviceName = settings.deviceName;
    if (deviceName.length() > 20) {
      deviceName = deviceName.substring(0, 20);
    }
    // Right-align with card's right edge (cardX=20 + cardW=200 = 220)
    int textWidth = deviceName.length() * 6;
    int xPos = 220 - textWidth;  // Text end aligns with card right edge
    canvas.drawString(deviceName.c_str(), xPos, 114);
  }

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawWiFiScan() {
  canvas.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Join Wi-Fi", 90, 30);

  // Calculate total items and scrolling window
  int totalItems = numSavedNetworks + numNetworks;
  const int maxVisible = 6; // Show up to 6 networks at once
  int startIndex = 0;

  // Center the selected item in the visible window
  if (totalItems > maxVisible) {
    startIndex = selectedNetworkIndex - (maxVisible / 2);
    if (startIndex < 0) startIndex = 0;
    if (startIndex > totalItems - maxVisible) startIndex = totalItems - maxVisible;
  }

  int yPos = 50;
  int endIndex = min(startIndex + maxVisible, totalItems);

  // Show scanning message if no networks yet
  if (totalItems == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_BLUE);
    canvas.drawString("Scanning networks...", 50, yPos + 10);

    int dots = (millis() / 300) % 4;
    for (int i = 0; i < dots; i++) {
      canvas.fillCircle(155 + (i * 8), yPos + 22, 2, TFT_BLUE);
    }
  } else {
    // Draw networks in the visible window
    for (int idx = startIndex; idx < endIndex; idx++) {
      // Draw separator line between saved and scanned networks
      if (idx == numSavedNetworks && numSavedNetworks > 0 && idx >= startIndex) {
        canvas.drawLine(10, yPos - 4, 230, yPos - 4, TFT_LIGHTGREY);
        yPos += 4; // Add some spacing after the line
      }

      bool isSelected = (idx == selectedNetworkIndex);

      if (isSelected) {
        canvas.fillRoundRect(5, yPos - 2, 230, 18, 3, TFT_LIGHTGREY);
      }

      canvas.setTextSize(2);
      canvas.setTextColor(TFT_BLACK);

      String displaySSID;
      bool isSaved = false;
      int signalStrength = 0;

      // Determine if this is a saved or scanned network
      if (idx < numSavedNetworks) {
        // Saved network
        displaySSID = savedSSIDs[idx];
        isSaved = true;
      } else {
        // Scanned network
        int scannedIdx = idx - numSavedNetworks;
        displaySSID = scannedNetworks[scannedIdx];
        signalStrength = scannedRSSI[scannedIdx];

        // Check if this scanned network is also saved
        for (int j = 0; j < numSavedNetworks; j++) {
          if (savedSSIDs[j] == displaySSID) {
            isSaved = true;
            break;
          }
        }
      }

      // Draw SSID with emoji support (like status bar)
      int textX = 15;
      int charCount = 0;
      const int maxChars = 18;  // Fewer chars at size 2

      for (int i = 0; i < displaySSID.length() && charCount < maxChars; i++) {
        uint8_t c = (uint8_t)displaySSID[i];

        if (c < 128 && textX < 200) {
          // Regular ASCII - draw it
          canvas.drawChar(c, textX, yPos);
          textX += 12;  // Wider spacing for size 2
          charCount++;
        } else if (c >= 0xF0 && i + 3 < displaySSID.length() && textX < 200) {
          // 4-byte emoji - draw icon (scaled up)
          char emojiBytes[5];
          emojiBytes[0] = displaySSID[i];
          emojiBytes[1] = displaySSID[i+1];
          emojiBytes[2] = displaySSID[i+2];
          emojiBytes[3] = displaySSID[i+3];
          emojiBytes[4] = 0;

          drawEmojiIcon(textX, yPos - 7, emojiBytes, TFT_BLACK, 2);  // Scale 2x, offset -7 to center 32px emoji in 18px row
          textX += 18; // Space for emoji icon (scaled)
          i += 3;
          charCount++;
        } else {
          // Skip unknown multi-byte sequences
          if (c >= 0xE0 && i + 2 < displaySSID.length()) i += 2;
          else if (c >= 0xC0 && i + 1 < displaySSID.length()) i += 1;
        }
      }

      // Draw indicators
      if (idx < numSavedNetworks) {
        // Saved indicator (green dot)
        canvas.fillCircle(230, yPos + 6, 3, TFT_GREEN);
      } else {
        // Signal bars for scanned networks
        int bars = map(signalStrength, -100, -50, 1, 4);
        for (int b = 0; b < bars; b++) {
          canvas.fillRect(215 + (b * 4), yPos + 12 - (b * 2), 3, 2 + (b * 2), isSaved ? TFT_GREEN : TFT_BLUE);
        }
      }

      yPos += 18;  // More spacing for size 2
    }

    // Show "Scanning..." if we have saved networks but no scanned networks yet
    if (numSavedNetworks > 0 && numNetworks == 0) {
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_BLUE);
      canvas.drawString("Scanning...", 85, yPos + 5);
    }
  }

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawWiFiPassword() {
  canvas.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Connect to:", 50, 30);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLUE);
  String displaySSID = targetSSID;
  if (displaySSID.length() > 30) {
    displaySSID = displaySSID.substring(0, 30) + "...";
  }
  canvas.drawString(displaySSID.c_str(), 10, 50);
  
  canvas.drawRect(5, 70, 230, 20, TFT_BLACK);
  canvas.drawRect(6, 71, 228, 18, TFT_BLACK);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_BLACK);
  String displayPW = inputPassword;
  if (displayPW.length() > 30) {
    displayPW = displayPW.substring(displayPW.length() - 30);
  }
  canvas.drawString(displayPW.c_str(), 10, 75);
  
  if ((millis() / 500) % 2 == 0) {
    int cursorX = 10 + (displayPW.length() * 6);
    canvas.drawLine(cursorX, 75, cursorX, 85, TFT_BLACK);
  }
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Enter=Connect `=Back Del=Delete", 20, 110);

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawWiFiSaved() {
  canvas.fillScreen(TFT_WHITE);
  drawStatusBar(false);

  canvas.setTextSize(2);
  canvas.setTextColor(TFT_BLACK);
  canvas.drawString("Saved Networks", 35, 30);
  
  if (numSavedNetworks == 0) {
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("No saved networks", 60, 70);
  } else {
    for (int i = 0; i < numSavedNetworks; i++) {
      int yPos = 50 + (i * 15);
      
      if (i == selectedSavedIndex) {
        canvas.fillRoundRect(5, yPos - 2, 230, 14, 5, TFT_LIGHTGREY);
      }
      
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_BLACK);
      
      String displaySSID = savedSSIDs[i];
      if (displaySSID.length() > 28) {
        displaySSID = displaySSID.substring(0, 28) + "...";
      }
      canvas.drawString(displaySSID.c_str(), 10, yPos);
      
      // Show saved indicator
      canvas.fillCircle(225, yPos + 5, 3, TFT_GREEN);
    }
  }
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString(",/=Nav Enter=Connect Del=Forget", 15, 120);
  canvas.drawString("`=Back", 95, 110);

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

// Music Menu
void drawMusicMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(true);  // Always dark theme for submenus

  // No "Music" title - removed

  // Draw menu items (starting higher without title)
  for (int i = 0; i < totalMusicItems; i++) {
    int yPos = 35 + (i * 20);  // Lifted up

    if (i == musicMenuIndex) {
      canvas.fillRoundRect(5, yPos - 2, 230, 18, 5, TFT_WHITE);  // White highlight
      canvas.setTextColor(TFT_BLACK);
    } else {
      // Use different purple shades for each item
      uint16_t itemColor = (i == 0) ? 0xF81F : (i == 1) ? 0xC99F : 0xA11F;  // Bright, light, medium purple
      canvas.setTextColor(itemColor);
    }

    canvas.setTextSize(1);
    canvas.drawString("> " + musicMenuItems[i].name, 10, yPos);
  }

  // Instructions
  canvas.setTextSize(1);
  canvas.setTextColor(0x780F);  // Deep purple
  canvas.drawString(",/=Navigate  Enter=Select", 30, 115);
  canvas.drawString("`=Back", 95, 125);

  // Push canvas to display
  canvas.pushSprite(0, 0);
}

void drawGamesMenu() {
  canvas.fillScreen(TFT_BLACK);
  drawStatusBar(true);  // Always dark theme for submenus

  // Menu items
  canvas.setTextSize(1);
  int yPos = 50;
  for (int i = 0; i < totalGamesItems; i++) {
    if (i == gamesMenuIndex) {
      // Selected item - draw with yellow highlight
      canvas.fillRect(10, yPos - 2, 220, 12, TFT_YELLOW);
      canvas.setTextColor(TFT_BLACK);
      canvas.drawString("> " + gamesMenuItems[i].name, 15, yPos);
    } else {
      // Unselected item
      canvas.setTextColor(gamesMenuItems[i].color);
      canvas.drawString("  " + gamesMenuItems[i].name, 15, yPos);
    }
    yPos += 15;
  }

  // Instructions
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("`;/. Navigate  Enter Select", 15, 110);

  // Push canvas to display
  canvas.pushSprite(0, 0);
}