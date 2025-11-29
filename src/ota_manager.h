#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "config.h"

#define GITHUB_API_URL "https://api.github.com/repos/fountainking/labDEVICE/releases/latest"

class OTAManager {
public:
    static bool checkForUpdate();
    static bool performUpdate(String firmwareURL);
    static String getCurrentVersion() { return FIRMWARE_VERSION; }

private:
    static void displayProgress(int progress, int total);
    static String parseJsonField(String json, String field);
};

#endif
