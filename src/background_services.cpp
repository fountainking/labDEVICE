#include "background_services.h"
#include "captive_portal.h"
#include "wifi_transfer.h"
#include <WiFi.h>

// Task handles
TaskHandle_t portalTaskHandle = NULL;
TaskHandle_t transferTaskHandle = NULL;

// Service status
static ServiceStatus currentStatus = {
  .fakeAPRunning = false,
  .portalRunning = false,
  .transferRunning = false,
  .fakeAPName = "",
  .portalName = "",
  .portalVisitors = 0,
  .connectedClients = 0
};

// FreeRTOS task for captive portal
void portalTask(void* parameter) {
  while (true) {
    if (currentStatus.portalRunning || isPortalRunning()) {
      handlePortalLoop();

      // Update status
      currentStatus.connectedClients = WiFi.softAPgetStationNum();
      currentStatus.portalVisitors = portalVisitorCount;

      // Sync status if portal was started directly
      if (isPortalRunning() && !currentStatus.portalRunning) {
        currentStatus.portalRunning = true;
        currentStatus.portalName = portalSSID;
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // 50ms delay - don't block audio
  }
}

// FreeRTOS task for file transfer server
// NOTE: This task is DISABLED - transfer is now handled directly in main loop
//       to avoid constant background processing that interferes with audio
void transferTask(void* parameter) {
  while (true) {
    if (currentStatus.transferRunning && serverRunning) {
      handleWebServerLoop();
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // 50ms delay - don't block audio
  }
}

void initBackgroundServices() {
  // DISABLED: All background tasks interfere with audio playback
  // Portal and transfer are now handled on-demand in main loop only when active

  // xTaskCreatePinnedToCore(
  //   portalTask,
  //   "PortalTask",
  //   4096,
  //   NULL,
  //   0,
  //   &portalTaskHandle,
  //   1
  // );

  // xTaskCreatePinnedToCore(
  //   transferTask,
  //   "TransferTask",
  //   4096,
  //   NULL,
  //   0,
  //   &transferTaskHandle,
  //   1
  // );
}

void startBackgroundFakeAP(const String& ssid) {
  stopBackgroundFakeAP(); // Stop any existing

  // Start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());

  currentStatus.fakeAPRunning = true;
  currentStatus.fakeAPName = ssid;
}

void startBackgroundPortal(const String& ssid) {
  stopBackgroundPortal(); // Stop any existing

  // Start portal (this sets up the web server and DNS)
  startCaptivePortal(ssid);

  currentStatus.portalRunning = true;
  currentStatus.portalName = ssid;
  currentStatus.portalVisitors = 0;
}

void startBackgroundTransfer() {
  stopBackgroundTransfer(); // Stop any existing

  if (WiFi.status() != WL_CONNECTED) {
    return; // Need WiFi connection
  }

  // Start transfer server
  startWebServer();

  currentStatus.transferRunning = true;
}

void stopBackgroundFakeAP() {
  if (currentStatus.fakeAPRunning) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF); // Save battery when AP stopped

    currentStatus.fakeAPRunning = false;
    currentStatus.fakeAPName = "";
  }
}

void stopBackgroundPortal() {
  if (currentStatus.portalRunning || isPortalRunning()) {
    stopCaptivePortal();
    WiFi.mode(WIFI_OFF); // Save battery when portal stopped

    currentStatus.portalRunning = false;
    currentStatus.portalName = "";
    currentStatus.portalVisitors = 0;
  }
}

void stopBackgroundTransfer() {
  if (currentStatus.transferRunning) {
    stopWebServer();

    currentStatus.transferRunning = false;
  }
}

void stopAllBackgroundServices() {
  stopBackgroundFakeAP();
  stopBackgroundPortal();
  stopBackgroundTransfer();
}

ServiceStatus getServiceStatus() {
  // Update connected clients count if any AP is running
  if (currentStatus.fakeAPRunning || currentStatus.portalRunning) {
    currentStatus.connectedClients = WiFi.softAPgetStationNum();
  }

  return currentStatus;
}

bool isAnyServiceRunning() {
  return currentStatus.fakeAPRunning ||
         currentStatus.portalRunning ||
         currentStatus.transferRunning;
}
