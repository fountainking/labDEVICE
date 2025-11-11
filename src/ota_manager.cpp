#include "ota_manager.h"
#include <M5Cardputer.h>
#include <WiFiClientSecure.h>

// Global static client to avoid heap fragmentation
static WiFiClientSecure secureClient;

// Star emoji - hardcoded for OTA UI
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

// Helper: Interpolate between blue and white based on line position
uint16_t gradientColor(int line, int totalLines) {
    // Blue (0x001F) to White (0xFFFF) gradient
    // RGB565: Blue = R:0, G:0, B:31 -> White = R:31, G:63, B:31
    float ratio = (float)line / (float)totalLines;

    uint8_t r = (uint8_t)(ratio * 31);
    uint8_t g = (uint8_t)(ratio * 63);
    uint8_t b = 31; // Keep blue channel max

    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

// Helper: Draw 2x scaled star emoji
void drawStarEmoji2x(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint16_t color = STAR_ICON[row][col];
            if (color != 0x07E0) { // Skip transparent pixels
                // Draw 2x2 block for each pixel
                M5Cardputer.Display.fillRect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}

// Helper: Wait for any key press and release (fixes restart bug)
void waitForKeyPressAndRelease() {
    // Wait for key press
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        yield(); // Feed watchdog
        delay(10);
    }

    // Wait for key release
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && !M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        yield(); // Feed watchdog
        delay(10);
    }
}

String OTAManager::parseJsonField(String json, String field) {
    String searchStr = "\"" + field + "\":\"";
    int startIdx = json.indexOf(searchStr);
    if (startIdx < 0) return "";

    startIdx += searchStr.length();
    int endIdx = json.indexOf("\"", startIdx);
    if (endIdx < 0) return "";

    return json.substring(startIdx, endIdx);
}

bool OTAManager::checkForUpdate() {
    M5Cardputer.Display.clear();

    // Draw 2x star emoji on right side (centered vertically)
    // Screen is 240x135, star is 32x32, so center at y=(135-32)/2 ≈ 51
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    M5Cardputer.Display.setTextSize(1);

    // Line 0: "Current: vX.X.X" - gradient blue to white
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(0, 5));
    M5Cardputer.Display.printf("Current: %s", FIRMWARE_VERSION);
    lineY += lineSpacing;

    // Line 1: "Checking for updates..."
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(1, 5));
    M5Cardputer.Display.println("Checking for updates...");
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation

    HTTPClient http;
    http.begin(secureClient, GITHUB_API_URL);
    http.addHeader("User-Agent", "M5Cardputer-Laboratory");
    http.setTimeout(15000); // 15 second timeout

    int httpCode = http.GET();

    if (httpCode != 200) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(2, 5));
        M5Cardputer.Display.printf("Error: HTTP %d", httpCode);
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(3, 5));
        M5Cardputer.Display.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse version tag
    String latestVersion = parseJsonField(payload, "tag_name");
    if (latestVersion.length() == 0) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(2, 5));
        M5Cardputer.Display.println("Parse error: No tag found");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(3, 5));
        M5Cardputer.Display.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(2, 5));
    M5Cardputer.Display.printf("Latest: %s", latestVersion.c_str());
    lineY += lineSpacing;

    // Compare versions
    if (latestVersion == String(FIRMWARE_VERSION)) {
        lineY += lineSpacing; // Extra space
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(3, 5));
        M5Cardputer.Display.println("Already up to date!");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(4, 5));
        M5Cardputer.Display.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    // Find firmware.bin download URL
    String searchStr = "\"browser_download_url\":\"";
    int urlStart = payload.indexOf(searchStr);
    if (urlStart < 0) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(3, 5));
        M5Cardputer.Display.println("No firmware.bin found");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(4, 5));
        M5Cardputer.Display.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    urlStart += searchStr.length();
    int urlEnd = payload.indexOf("\"", urlStart);
    String firmwareUrl = payload.substring(urlStart, urlEnd);

    // Confirm only if we find firmware.bin in the URL
    if (firmwareUrl.indexOf("firmware.bin") < 0) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(3, 5));
        M5Cardputer.Display.println("Invalid firmware URL");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(4, 5));
        M5Cardputer.Display.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    lineY += lineSpacing; // Extra space
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(3, 5));
    M5Cardputer.Display.println("Update available!");
    lineY += lineSpacing;
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(4, 5));
    M5Cardputer.Display.println("Press ENTER to install");
    lineY += lineSpacing;
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.println("Press ESC to cancel");

    // Wait for user input
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                if (status.enter) {
                    return performUpdate(firmwareUrl);
                }

                // Check for ESC key (backtick)
                for (auto key : status.word) {
                    if (key == '`') {
                        lineY += lineSpacing;
                        M5Cardputer.Display.setCursor(15, lineY);
                        M5Cardputer.Display.setTextColor(WHITE);
                        M5Cardputer.Display.println("Cancelled.");
                        delay(1000);
                        return false;
                    }
                }
            }
        }
        yield(); // Feed watchdog
        delay(10);
    }

    return false;
}

bool OTAManager::performUpdate(String firmwareURL) {
    M5Cardputer.Display.clear();

    // Draw 2x star emoji on right side (centered vertically)
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    M5Cardputer.Display.setTextSize(1);

    // Line 0: "Downloading firmware..."
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(0, 5));
    M5Cardputer.Display.println("Downloading firmware...");
    lineY += lineSpacing;

    // Line 1: Firmware URL (truncate if too long)
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(1, 5));
    String displayURL = firmwareURL;
    if (displayURL.length() > 35) {
        displayURL = displayURL.substring(0, 32) + "...";
    }
    M5Cardputer.Display.println(displayURL);
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation
    secureClient.setHandshakeTimeout(30); // 30 second TLS timeout

    lineY += lineSpacing; // Extra space
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(2, 5));
    M5Cardputer.Display.println("Connecting...");
    lineY += lineSpacing;

    HTTPClient http;
    http.begin(secureClient, firmwareURL);
    http.setTimeout(60000); // 60 second timeout for download
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(3, 5));
    M5Cardputer.Display.println("Requesting firmware...");
    lineY += lineSpacing;

    int httpCode = http.GET();

    if (httpCode != 200) {
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(gradientColor(4, 5));
        M5Cardputer.Display.printf("Download failed: %d", httpCode);
        lineY += lineSpacing;
        if (httpCode == -1) {
            M5Cardputer.Display.setCursor(15, lineY);
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.println("Connection error");
            lineY += lineSpacing;
            M5Cardputer.Display.setCursor(15, lineY);
            M5Cardputer.Display.println("Check WiFi signal");
            lineY += lineSpacing;
        }
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(WHITE);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    int contentLength = http.getSize();
    lineY += lineSpacing;
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(gradientColor(4, 5));
    M5Cardputer.Display.printf("Size: %d bytes", contentLength);
    lineY += lineSpacing;

    if (contentLength <= 0) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(WHITE);
        M5Cardputer.Display.println("Invalid content length");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.setTextColor(WHITE);
        M5Cardputer.Display.println("Not enough space!");
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.printf("Needed: %d bytes", contentLength);
        lineY += lineSpacing;
        M5Cardputer.Display.setCursor(15, lineY);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    lineY += lineSpacing;
    M5Cardputer.Display.setCursor(15, lineY);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.println("Flashing firmware...");

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[128];
    int lastProgress = -1;

    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        if (available) {
            int c = stream->readBytes(buff, min(available, sizeof(buff)));
            Update.write(buff, c);
            written += c;

            int progress = (written * 100) / contentLength;
            if (progress != lastProgress) {
                displayProgress(written, contentLength);
                lastProgress = progress;
            }
        }
        delay(1);
    }

    http.end();

    if (written != contentLength) {
        M5Cardputer.Display.setCursor(15, 100);
        M5Cardputer.Display.setTextColor(WHITE);
        M5Cardputer.Display.printf("Write incomplete!");
        M5Cardputer.Display.setCursor(15, 110);
        M5Cardputer.Display.printf("Written: %d / %d", written, contentLength);
        M5Cardputer.Display.setCursor(15, 120);
        M5Cardputer.Display.println("Press any key...");
        Update.abort();
        waitForKeyPressAndRelease();
        return false;
    }

    if (Update.end()) {
        if (Update.isFinished()) {
            M5Cardputer.Display.setCursor(15, 100);
            M5Cardputer.Display.setTextColor(WHITE);
            M5Cardputer.Display.println("Update complete!");
            M5Cardputer.Display.setCursor(15, 110);
            M5Cardputer.Display.println("Rebooting in 3s...");
            delay(3000);
            ESP.restart();
            return true;
        }
    }

    M5Cardputer.Display.setCursor(15, 100);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.println("Update failed!");
    M5Cardputer.Display.setCursor(15, 110);
    M5Cardputer.Display.printf("Error: %s", Update.errorString());
    M5Cardputer.Display.setCursor(15, 120);
    M5Cardputer.Display.println("Press any key...");
    waitForKeyPressAndRelease();
    return false;
}

void OTAManager::displayProgress(int current, int total) {
    int percent = (current * 100) / total;
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = 81; // Moved down 1px from 80

    // Draw progress bar background
    M5Cardputer.Display.drawRect(barX, barY, barWidth, barHeight, WHITE);

    // Draw progress bar fill (YELLOW = 0xFFE0)
    int fillWidth = (barWidth * current) / total;
    M5Cardputer.Display.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, 0xFFE0);

    // Draw percentage text
    M5Cardputer.Display.setCursor(barX + barWidth / 2 - 20, barY + barHeight + 10);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.printf("%d%%", percent);
}
