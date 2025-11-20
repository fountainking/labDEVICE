#include "ota_manager.h"
#include "ui.h"
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
                canvas.fillRect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
  // Push canvas to display
  canvas.pushSprite(0, 0);
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
    canvas.clear();

    // Draw 2x star emoji on right side (centered vertically)
    // Screen is 240x135, star is 32x32, so center at y=(135-32)/2 ≈ 51
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    canvas.setTextSize(1);

    // Line 0: "Current: vX.X.X" - gradient blue to white
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(0, 5));
    canvas.printf("Current: %s", FIRMWARE_VERSION);
    lineY += lineSpacing;

    // Line 1: "Checking for updates..."
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(1, 5));
    canvas.println("Checking for updates...");
    lineY += lineSpacing;

    // Debug: Check WiFi status
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        canvas.println("WiFi not connected!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }
    canvas.printf("WiFi: %s", WiFi.SSID().c_str());
    lineY += lineSpacing;

    // Debug: Show RSSI
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(3, 5));
    canvas.printf("Signal: %d dBm", WiFi.RSSI());
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation
    secureClient.setHandshakeTimeout(60); // 60 second TLS timeout

    // Debug: Connecting
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(4, 5));
    canvas.println("Connecting to GitHub...");
    lineY += lineSpacing;

    HTTPClient http;
    http.begin(secureClient, GITHUB_API_URL);
    http.addHeader("User-Agent", "M5Cardputer-Laboratory");
    http.setTimeout(60000); // 60 second timeout

    canvas.setCursor(15, lineY);
    canvas.setTextColor(WHITE);
    canvas.println("Sending GET request...");
    lineY += lineSpacing;

    int httpCode = http.GET();

    if (httpCode != 200) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(2, 5));
        canvas.printf("Error: HTTP %d", httpCode);
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse version tag
    String latestVersion = parseJsonField(payload, "tag_name");
    if (latestVersion.length() == 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(2, 5));
        canvas.println("Parse error: No tag found");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    canvas.printf("Latest: %s", latestVersion.c_str());
    lineY += lineSpacing;

    // Compare versions
    if (latestVersion == String(FIRMWARE_VERSION)) {
        lineY += lineSpacing; // Extra space
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Already up to date!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    // Find firmware.bin download URL
    String searchStr = "\"browser_download_url\":\"";
    int urlStart = payload.indexOf(searchStr);
    if (urlStart < 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("No firmware.bin found");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    urlStart += searchStr.length();
    int urlEnd = payload.indexOf("\"", urlStart);
    String firmwareUrl = payload.substring(urlStart, urlEnd);

    // Confirm only if we find firmware.bin in the URL
    if (firmwareUrl.indexOf("firmware.bin") < 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Invalid firmware URL");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }

    // Check if update is mandatory
    String releaseName = parseJsonField(payload, "name");
    bool isMandatory = (releaseName.indexOf("[MANDATORY]") >= 0) ||
                       (releaseName.indexOf("[CRITICAL]") >= 0);

    lineY += lineSpacing; // Extra space

    if (isMandatory) {
        // MANDATORY UPDATE - auto-install, no ESC
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_RED);
        canvas.println("CRITICAL UPDATE REQUIRED");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_YELLOW);
        canvas.println("Installing automatically...");
        delay(2000); // Give user time to read
        return performUpdate(firmwareUrl);
    }

    // Optional update - show ENTER/ESC options
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(3, 5));
    canvas.println("Update available!");
    lineY += lineSpacing;
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(4, 5));
    canvas.println("Press ENTER to install");
    lineY += lineSpacing;
    canvas.setCursor(15, lineY);
    canvas.setTextColor(WHITE);
    canvas.println("Press ESC to cancel");

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
                        canvas.setCursor(15, lineY);
                        canvas.setTextColor(WHITE);
                        canvas.println("Cancelled.");
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
    canvas.clear();

    // Draw 2x star emoji on right side (centered vertically)
    drawStarEmoji2x(240 - 32 - 10, 51);

    // Left margin: 15px, Top margin: 15px
    int lineY = 15;
    int lineSpacing = 10;

    canvas.setTextSize(1);

    // Line 0: "Downloading firmware..."
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(0, 5));
    canvas.println("Downloading firmware...");
    lineY += lineSpacing;

    // Line 1: Firmware URL (truncate if too long)
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(1, 5));
    String displayURL = firmwareURL;
    if (displayURL.length() > 35) {
        displayURL = displayURL.substring(0, 32) + "...";
    }
    canvas.println(displayURL);
    lineY += lineSpacing;

    // Debug: WiFi check
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        canvas.println("WiFi disconnected!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        waitForKeyPressAndRelease();
        return false;
    }
    canvas.printf("WiFi OK (%d dBm)", WiFi.RSSI());
    lineY += lineSpacing;

    secureClient.setInsecure(); // Skip certificate validation
    secureClient.setHandshakeTimeout(60); // 60 second TLS timeout

    lineY += lineSpacing; // Extra space
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(3, 5));
    canvas.println("TLS handshake...");
    lineY += lineSpacing;

    HTTPClient http;
    http.begin(secureClient, firmwareURL);
    http.setTimeout(60000); // 60 second timeout for download
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(4, 5));
    canvas.println("Requesting firmware...");
    lineY += lineSpacing;

    int httpCode = http.GET();

    canvas.setCursor(15, lineY);
    canvas.setTextColor(WHITE);
    canvas.printf("Response: %d", httpCode);
    lineY += lineSpacing;

    if (httpCode != 200) {
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.printf("Download failed: %d", httpCode);
        lineY += lineSpacing;
        if (httpCode == -1) {
            canvas.setCursor(15, lineY);
            canvas.setTextColor(WHITE);
            canvas.println("Connection error");
            lineY += lineSpacing;
            canvas.setCursor(15, lineY);
            canvas.println("Check WiFi signal");
            lineY += lineSpacing;
        }
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    int contentLength = http.getSize();
    lineY += lineSpacing;
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(4, 5));
    canvas.printf("Size: %d bytes", contentLength);
    lineY += lineSpacing;

    if (contentLength <= 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.println("Invalid content length");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.println("Not enough space!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.printf("Needed: %d bytes", contentLength);
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        http.end();
        waitForKeyPressAndRelease();
        return false;
    }

    // Remove "Flashing firmware..." text - progress bar will be at bottom

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
        canvas.setCursor(15, 100);
        canvas.setTextColor(WHITE);
        canvas.printf("Write incomplete!");
        canvas.setCursor(15, 110);
        canvas.printf("Written: %d / %d", written, contentLength);
        canvas.setCursor(15, 120);
        canvas.println("Press any key...");
        Update.abort();
        waitForKeyPressAndRelease();
        return false;
    }

    if (Update.end()) {
        if (Update.isFinished()) {
            canvas.setCursor(15, 100);
            canvas.setTextColor(WHITE);
            canvas.println("Update complete!");
            canvas.setCursor(15, 110);
            canvas.println("Rebooting in 3s...");
            delay(3000);
            ESP.restart();
            return true;
        }
    }

    canvas.setCursor(15, 100);
    canvas.setTextColor(WHITE);
    canvas.println("Update failed!");
    canvas.setCursor(15, 110);
    canvas.printf("Error: %s", Update.errorString());
    canvas.setCursor(15, 120);
    canvas.println("Press any key...");
    waitForKeyPressAndRelease();
    return false;
}

void OTAManager::displayProgress(int current, int total) {
    int percent = (current * 100) / total;
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = 115; // Bottom of screen (135 - 20)

    // Draw progress bar background
    canvas.drawRect(barX, barY, barWidth, barHeight, WHITE);

    // Draw progress bar fill (YELLOW = 0xFFE0)
    int fillWidth = (barWidth * current) / total;
    canvas.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, 0xFFE0);

    // Draw percentage text centered inside the bar
    canvas.setTextSize(1);
    canvas.setCursor(barX + barWidth / 2 - 12, barY + 6);
    canvas.setTextColor(BLACK, 0xFFE0); // Black text on yellow background
    canvas.printf("%d%%", percent);
}
