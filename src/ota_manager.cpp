#include "ota_manager.h"
#include "ui.h"
#include <M5Cardputer.h>
#include <WiFiClientSecure.h>

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
    float ratio = (float)line / (float)totalLines;
    uint8_t r = (uint8_t)(ratio * 31);
    uint8_t g = (uint8_t)(ratio * 63);
    uint8_t b = 31;
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

// Helper: Draw 2x scaled star emoji (does NOT push - caller must push after all drawing)
void drawStarEmoji2x(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint16_t color = STAR_ICON[row][col];
            if (color != 0x07E0) {
                canvas.fillRect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}

// Helper: Wait for any key press and release
void waitForKeyPressAndRelease() {
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        yield();
        delay(10);
    }
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && !M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        yield();
        delay(10);
    }
}

String OTAManager::parseJsonField(String json, String field) {
    String searchStr = "\"" + field + "\":\"";
    int startIdx = json.indexOf(searchStr);
    if (startIdx < 0) {
        searchStr = "\"" + field + "\": \"";
        startIdx = json.indexOf(searchStr);
    }
    if (startIdx < 0) return "";
    startIdx += searchStr.length();
    int endIdx = json.indexOf("\"", startIdx);
    if (endIdx < 0) return "";
    return json.substring(startIdx, endIdx);
}

bool OTAManager::checkForUpdate() {
    canvas.clear();
    drawStarEmoji2x(240 - 32 - 10, 51);

    int lineY = 15;
    int lineSpacing = 10;

    canvas.setTextSize(1);

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(0, 5));
    canvas.printf("Current: %s", FIRMWARE_VERSION);
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(1, 5));
    canvas.println("Checking for updates...");
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        canvas.println("WiFi not connected!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }
    canvas.printf("WiFi: %s", WiFi.SSID().c_str());
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(3, 5));
    canvas.printf("Signal: %d dBm", WiFi.RSSI());
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(4, 5));
    canvas.println("Connecting to GitHub...");
    lineY += lineSpacing;
    canvas.pushSprite(0, 0);

    // Retry loop - up to 3 attempts with fresh client each time
    int httpCode = -1;
    String payload;
    const int maxRetries = 3;

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.printf("Attempt %d/%d...", attempt, maxRetries);
        canvas.pushSprite(0, 0);

        // Create fresh client with heap allocation for each attempt
        WiFiClientSecure* client = new WiFiClientSecure();
        client->setInsecure();
        client->setHandshakeTimeout(30);

        // Manual connection to api.github.com
        canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.printf("Connecting... (%d)", attempt);
        canvas.pushSprite(0, 0);

        if (!client->connect("api.github.com", 443)) {
            delete client;
            canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
            canvas.setCursor(15, lineY);
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("TLS connect failed");
            canvas.pushSprite(0, 0);
            delay(2000);
            continue;
        }

        HTTPClient http;
        http.setReuse(false);

        if (!http.begin(*client, GITHUB_API_URL)) {
            client->stop();
            delete client;
            canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
            canvas.setCursor(15, lineY);
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("Begin failed");
            canvas.pushSprite(0, 0);
            delay(1000);
            continue;
        }

        http.addHeader("User-Agent", "ESP32");
        http.setTimeout(20000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        httpCode = http.GET();

        if (httpCode == 200) {
            // Read payload while connection is still active
            int len = http.getSize();
            WiFiClient* stream = http.getStreamPtr();

            payload = "";
            payload.reserve(len > 0 ? len : 4096);

            uint8_t buff[256];
            while (http.connected() && (len > 0 || len == -1)) {
                size_t avail = stream->available();
                if (avail) {
                    int c = stream->readBytes(buff, min(avail, sizeof(buff)));
                    payload += String((char*)buff).substring(0, c);
                    if (len > 0) len -= c;
                }
                yield();
            }

            http.end();
            client->stop();
            delete client;
            break;
        }

        http.end();
        client->stop();
        delete client;

        if (attempt < maxRetries) {
            canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
            canvas.setCursor(15, lineY);
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("Failed (%d), retrying...", httpCode);
            canvas.pushSprite(0, 0);
            delay(2000);
        }
    }

    lineY += lineSpacing;

    if (httpCode != 200) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(2, 5));
        canvas.printf("Error: HTTP %d", httpCode);
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_YELLOW);
        if (httpCode == -1) {
            canvas.println("Connection failed");
        } else {
            canvas.println("Server error");
        }
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }

    canvas.setCursor(15, lineY);
    canvas.setTextColor(WHITE);
    canvas.printf("Got %d bytes", payload.length());
    lineY += lineSpacing;
    canvas.pushSprite(0, 0);

    String latestVersion = parseJsonField(payload, "tag_name");
    if (latestVersion.length() == 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(2, 5));
        canvas.println("Parse error: No tag found");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_YELLOW);
        String preview = payload.substring(0, 35);
        canvas.println(preview);
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    canvas.printf("Latest: %s", latestVersion.c_str());
    lineY += lineSpacing;

    if (latestVersion == String(FIRMWARE_VERSION)) {
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Already up to date!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }

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
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }

    urlStart += searchStr.length();
    int urlEnd = payload.indexOf("\"", urlStart);
    String firmwareUrl = payload.substring(urlStart, urlEnd);

    if (firmwareUrl.indexOf("firmware.bin") < 0) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(3, 5));
        canvas.println("Invalid firmware URL");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(gradientColor(4, 5));
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }

    String releaseName = parseJsonField(payload, "name");
    bool isMandatory = (releaseName.indexOf("[MANDATORY]") >= 0) ||
                       (releaseName.indexOf("[CRITICAL]") >= 0);

    lineY += lineSpacing;

    if (isMandatory) {
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_RED);
        canvas.println("CRITICAL UPDATE REQUIRED");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.setTextColor(TFT_YELLOW);
        canvas.println("Installing automatically...");
        canvas.pushSprite(0, 0);
        delay(2000);
        return performUpdate(firmwareUrl);
    }

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
    canvas.pushSprite(0, 0);

    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
                if (status.enter) {
                    return performUpdate(firmwareUrl);
                }
                for (auto key : status.word) {
                    if (key == '`') {
                        lineY += lineSpacing;
                        canvas.setCursor(15, lineY);
                        canvas.setTextColor(WHITE);
                        canvas.println("Cancelled.");
                        canvas.pushSprite(0, 0);
                        delay(1000);
                        return false;
                    }
                }
            }
        }
        yield();
        delay(10);
    }

    return false;
}

bool OTAManager::performUpdate(String firmwareURL) {
    canvas.clear();
    drawStarEmoji2x(240 - 32 - 10, 51);

    int lineY = 15;
    int lineSpacing = 10;

    canvas.setTextSize(1);

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(0, 5));
    canvas.println("Downloading firmware...");
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(1, 5));
    String displayURL = firmwareURL;
    if (displayURL.length() > 35) {
        displayURL = displayURL.substring(0, 32) + "...";
    }
    canvas.println(displayURL);
    lineY += lineSpacing;

    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(2, 5));
    if (WiFi.status() != WL_CONNECTED) {
        canvas.println("WiFi disconnected!");
        lineY += lineSpacing;
        canvas.setCursor(15, lineY);
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
        waitForKeyPressAndRelease();
        return false;
    }
    canvas.printf("WiFi OK (%d dBm)", WiFi.RSSI());
    lineY += lineSpacing;

    lineY += lineSpacing;
    canvas.setCursor(15, lineY);
    canvas.setTextColor(gradientColor(3, 5));
    canvas.println("Connecting...");
    lineY += lineSpacing;
    canvas.pushSprite(0, 0);

    // Retry loop with fresh client each time
    int httpCode = -1;
    const int maxRetries = 3;
    WiFiClientSecure* pClient = nullptr;
    HTTPClient http;

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
        canvas.setCursor(15, lineY);
        canvas.setTextColor(WHITE);
        canvas.printf("Download attempt %d/%d...", attempt, maxRetries);
        canvas.pushSprite(0, 0);

        // Create fresh client
        pClient = new WiFiClientSecure();
        pClient->setInsecure();
        pClient->setTimeout(30);

        http.setReuse(false);

        if (!http.begin(*pClient, firmwareURL)) {
            delete pClient;
            pClient = nullptr;
            canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
            canvas.setCursor(15, lineY);
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("Begin failed...");
            canvas.pushSprite(0, 0);
            delay(1000);
            continue;
        }

        http.setTimeout(60000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        httpCode = http.GET();

        if (httpCode == 200) {
            break;
        }

        http.end();
        delete pClient;
        pClient = nullptr;

        if (attempt < maxRetries) {
            canvas.fillRect(15, lineY, 180, 10, TFT_BLACK);
            canvas.setCursor(15, lineY);
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("Attempt %d failed (%d)...", attempt, httpCode);
            canvas.pushSprite(0, 0);
            delay(2000);
        }
    }

    lineY += lineSpacing;
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
        canvas.pushSprite(0, 0);
        if (pClient) delete pClient;
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
        canvas.pushSprite(0, 0);
        http.end();
        if (pClient) delete pClient;
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
        canvas.pushSprite(0, 0);
        http.end();
        if (pClient) delete pClient;
        waitForKeyPressAndRelease();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[512];
    int lastProgress = -1;

    while (http.connected() && (written < (size_t)contentLength)) {
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
        yield();
        delay(1);
    }

    http.end();
    if (pClient) delete pClient;

    if (written != (size_t)contentLength) {
        canvas.setCursor(15, 100);
        canvas.setTextColor(WHITE);
        canvas.printf("Write incomplete!");
        canvas.setCursor(15, 110);
        canvas.printf("Written: %d / %d", written, contentLength);
        canvas.setCursor(15, 120);
        canvas.println("Press any key...");
        canvas.pushSprite(0, 0);
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
            canvas.pushSprite(0, 0);
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
    canvas.pushSprite(0, 0);
    waitForKeyPressAndRelease();
    return false;
}

void OTAManager::displayProgress(int current, int total) {
    int percent = (current * 100) / total;
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = 115;

    canvas.drawRect(barX, barY, barWidth, barHeight, WHITE);

    int fillWidth = (barWidth * current) / total;
    canvas.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, 0xFFE0);

    canvas.setTextSize(1);
    canvas.setCursor(barX + barWidth / 2 - 12, barY + 6);
    canvas.setTextColor(BLACK, 0xFFE0);
    canvas.printf("%d%%", percent);
    canvas.pushSprite(0, 0);
}
