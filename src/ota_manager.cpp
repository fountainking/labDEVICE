#include "ota_manager.h"
#include <M5Cardputer.h>
#include <WiFiClientSecure.h>

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
    M5Cardputer.Display.setCursor(10, 10);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.printf("Current: %s\n", FIRMWARE_VERSION);
    M5Cardputer.Display.println("Checking for updates...");

    WiFiClientSecure *client = new WiFiClientSecure;
    if (!client) {
        M5Cardputer.Display.println("Failed to create client");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    client->setInsecure(); // Skip certificate validation

    HTTPClient http;
    http.begin(*client, GITHUB_API_URL);
    http.addHeader("User-Agent", "M5Cardputer-Laboratory");
    http.setTimeout(15000); // 15 second timeout

    int httpCode = http.GET();

    if (httpCode != 200) {
        M5Cardputer.Display.printf("Error: HTTP %d\n", httpCode);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        delete client;
        return false;
    }

    String payload = http.getString();
    http.end();
    delete client;

    // Parse version tag
    String latestVersion = parseJsonField(payload, "tag_name");
    if (latestVersion.length() == 0) {
        M5Cardputer.Display.println("Parse error: No tag found");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    M5Cardputer.Display.printf("Latest: %s\n", latestVersion.c_str());

    // Compare versions
    if (latestVersion == String(FIRMWARE_VERSION)) {
        M5Cardputer.Display.println("\nAlready up to date!");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    // Find firmware.bin download URL
    String searchStr = "\"browser_download_url\":\"";
    int urlStart = payload.indexOf(searchStr);
    if (urlStart < 0) {
        M5Cardputer.Display.println("No firmware.bin found");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    urlStart += searchStr.length();
    int urlEnd = payload.indexOf("\"", urlStart);
    String firmwareUrl = payload.substring(urlStart, urlEnd);

    // Confirm only if we find firmware.bin in the URL
    if (firmwareUrl.indexOf("firmware.bin") < 0) {
        M5Cardputer.Display.println("Invalid firmware URL");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    M5Cardputer.Display.println("\nUpdate available!");
    M5Cardputer.Display.println("Press ENTER to install");
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
                        M5Cardputer.Display.println("\nCancelled.");
                        delay(1000);
                        return false;
                    }
                }
            }
        }
        delay(10);
    }

    return false;
}

bool OTAManager::performUpdate(String firmwareURL) {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setCursor(10, 10);
    M5Cardputer.Display.println("Downloading firmware...");
    M5Cardputer.Display.println(firmwareURL);

    WiFiClientSecure *client = new WiFiClientSecure;
    if (!client) {
        M5Cardputer.Display.println("\nFailed to create client");
        M5Cardputer.Display.println("Press any key...");
        return false;
    }

    client->setInsecure(); // Skip certificate validation

    HTTPClient http;
    http.begin(*client, firmwareURL);
    http.setTimeout(30000); // 30 second timeout for download

    int httpCode = http.GET();
    if (httpCode != 200) {
        M5Cardputer.Display.printf("\nDownload failed: %d\n", httpCode);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        delete client;
        return false;
    }

    int contentLength = http.getSize();
    M5Cardputer.Display.printf("\nSize: %d bytes\n", contentLength);

    if (contentLength <= 0) {
        M5Cardputer.Display.println("Invalid content length");
        M5Cardputer.Display.println("Press any key...");
        http.end();
        delete client;
        return false;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        M5Cardputer.Display.println("Not enough space!");
        M5Cardputer.Display.printf("Needed: %d bytes\n", contentLength);
        M5Cardputer.Display.println("Press any key...");
        http.end();
        delete client;
        return false;
    }

    M5Cardputer.Display.println("\nFlashing firmware...");

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
    delete client;

    if (written != contentLength) {
        M5Cardputer.Display.printf("\nWrite incomplete!\n");
        M5Cardputer.Display.printf("Written: %d / %d\n", written, contentLength);
        M5Cardputer.Display.println("Press any key...");
        Update.abort();
        return false;
    }

    if (Update.end()) {
        if (Update.isFinished()) {
            M5Cardputer.Display.println("\n\nUpdate complete!");
            M5Cardputer.Display.println("Rebooting in 3s...");
            delay(3000);
            ESP.restart();
            return true;
        }
    }

    M5Cardputer.Display.println("\nUpdate failed!");
    M5Cardputer.Display.printf("Error: %s\n", Update.errorString());
    M5Cardputer.Display.println("Press any key...");
    return false;
}

void OTAManager::displayProgress(int current, int total) {
    int percent = (current * 100) / total;
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = 80;

    // Draw progress bar background
    M5Cardputer.Display.drawRect(barX, barY, barWidth, barHeight, WHITE);

    // Draw progress bar fill
    int fillWidth = (barWidth * current) / total;
    M5Cardputer.Display.fillRect(barX + 2, barY + 2, fillWidth - 4, barHeight - 4, GREEN);

    // Draw percentage text
    M5Cardputer.Display.setCursor(barX + barWidth / 2 - 20, barY + barHeight + 10);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.printf("%d%%", percent);
}
