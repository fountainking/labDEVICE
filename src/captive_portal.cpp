#include "captive_portal.h"
#include "ui.h"
#include "settings.h"
#include "portal_manager.h"
#include "file_manager.h"

// Portal globals
PortalState portalState = PORTAL_STOPPED;
WebServer* portalWebServer = nullptr;
DNSServer* portalDNS = nullptr;
int portalVisitorCount = 0;
String portalSSID = "";
unsigned long portalStartTime = 0;

// Track unique visitors by IP
#define MAX_VISITORS 50
IPAddress visitorIPs[MAX_VISITORS];
int uniqueVisitorCount = 0;

// Helper: Check if visitor is new
bool isNewVisitor(IPAddress ip) {
    for (int i = 0; i < uniqueVisitorCount; i++) {
        if (visitorIPs[i] == ip) return false;
    }
    if (uniqueVisitorCount < MAX_VISITORS) {
        visitorIPs[uniqueVisitorCount++] = ip;
        portalVisitorCount++;
        return true;
    }
    return false;
}

// Simple HTML page - placeholder for Laboratory pitch deck
const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Laboratory</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
        }
        .container {
            max-width: 600px;
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }
        h1 {
            font-size: 3em;
            margin: 0 0 10px 0;
            text-align: center;
        }
        h2 {
            font-size: 1.5em;
            margin: 0 0 30px 0;
            text-align: center;
            opacity: 0.9;
        }
        p {
            font-size: 1.1em;
            line-height: 1.6;
            text-align: center;
            opacity: 0.9;
        }
        .cta {
            margin-top: 30px;
            text-align: center;
        }
        button {
            background: white;
            color: #667eea;
            border: none;
            padding: 15px 40px;
            font-size: 1.1em;
            font-weight: 600;
            border-radius: 50px;
            cursor: pointer;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0,0,0,0.3);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Laboratory</h1>
        <h2>Workforce Development & Innovation</h2>
        <p>Building the future through education, technology, and creative solutions.</p>
        <p>This is a demonstration of our M5Cardputer business card system.</p>
        <div class="cta">
            <button onclick="alert('Contact functionality coming soon!')">Get In Touch</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

void startCaptivePortal(const String& ssid) {
    stopCaptivePortal(); // Stop any existing portal

    // Ensure SD card is mounted for serving fonts/videos
    if (!sdCardMounted) {
        sdCardMounted = (SD.cardType() != CARD_NONE);
    }

    portalSSID = ssid;
    portalVisitorCount = 0;
    uniqueVisitorCount = 0; // Reset visitor tracking
    portalStartTime = millis();

    // Configure Access Point with custom IP configuration
    WiFi.mode(WIFI_AP);

    // Set custom AP IP configuration for better captive portal detection
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    // Parameters: ssid, password, channel, hidden, max_connections
    // Max connections bumped to 10 (ESP32 hardware limit)
    WiFi.softAP(ssid.c_str(), NULL, 6, 0, 10);

    delay(100);

    IPAddress IP = WiFi.softAPIP();

    // Start DNS server (captures all DNS requests)
    portalDNS = new DNSServer();
    portalDNS->setTTL(30); // Short TTL for faster portal detection
    portalDNS->start(53, "*", IP); // Redirect everything to our IP

    // Start web server
    portalWebServer = new WebServer(80);

    // Handle all requests with the same HTML page
    portalWebServer->onNotFound([]() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", PORTAL_HTML);
    });

    // Root handler
    portalWebServer->on("/", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", PORTAL_HTML);
    });

    // Captive portal detection endpoints
    // Android - redirect to portal to trigger popup
    portalWebServer->on("/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/gen_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Additional Android connectivity checks
    portalWebServer->on("/connectivitycheck.gstatic.com/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // iOS/Apple - redirect to portal to trigger popup
    portalWebServer->on("/hotspot-detect.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/library/test/success.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Windows - special case: redirect to logout.net
    portalWebServer->on("/connecttest.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://logout.net", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/ncsi.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Ubuntu/Linux
    portalWebServer->on("/canonical.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/connectivity-check.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Firefox
    portalWebServer->on("/success.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // iOS captive portal success endpoint - signals portal completion
    portalWebServer->on("/connected", []() {
        // Redirect to Apple's success URL to close captive portal
        portalWebServer->sendHeader("Location", "http://captive.apple.com/hotspot-detect.html", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Serve font file from SD card (/fonts/automate_light.ttf)
    portalWebServer->on("/automate_light.ttf", []() {
        if (sdCardMounted) {
            File fontFile = SD.open("/fonts/automate_light.ttf");
            if (fontFile) {
                portalWebServer->sendHeader("Cache-Control", "max-age=86400");
                portalWebServer->streamFile(fontFile, "font/ttf");
                fontFile.close();
                return;
            }
        }
        portalWebServer->send(404, "text/plain", "Font not found");
    });

    portalWebServer->begin();

    portalState = PORTAL_RUNNING;
    drawPortalScreen();
}

// Global variable to store custom HTML
String customPortalHTML = "";

void startCaptivePortalFromProfile(const PortalProfile& profile) {
    stopCaptivePortal(); // Stop any existing portal

    // Ensure SD card is mounted for serving fonts/videos
    if (!sdCardMounted) {
        sdCardMounted = (SD.cardType() != CARD_NONE);
    }

    portalSSID = profile.ssid;
    portalVisitorCount = 0;
    uniqueVisitorCount = 0; // Reset visitor tracking
    portalStartTime = millis();

    // Load custom HTML from profile
    customPortalHTML = loadPortalHTML(profile);

    // Configure Access Point with custom IP configuration
    WiFi.mode(WIFI_AP);

    // Set custom AP IP configuration for better captive portal detection
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    // Parameters: ssid, password, channel, hidden, max_connections
    // Max connections bumped to 10 (ESP32 hardware limit)
    WiFi.softAP(profile.ssid.c_str(), NULL, 6, 0, 10);

    delay(100);

    IPAddress IP = WiFi.softAPIP();

    // Start DNS server (captures all DNS requests)
    portalDNS = new DNSServer();
    portalDNS->setTTL(30); // Short TTL for faster portal detection
    portalDNS->start(53, "*", IP); // Redirect everything to our IP

    // Start web server
    portalWebServer = new WebServer(80);

    // Handle all requests with custom HTML
    portalWebServer->onNotFound([]() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    // Root handler
    portalWebServer->on("/", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    // Captive portal detection endpoints
    // Android - redirect to portal to trigger popup
    portalWebServer->on("/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/gen_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Additional Android connectivity checks
    portalWebServer->on("/connectivitycheck.gstatic.com/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // iOS/Apple - redirect to portal to trigger popup
    portalWebServer->on("/hotspot-detect.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/library/test/success.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Windows - special case: redirect to logout.net
    portalWebServer->on("/connecttest.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://logout.net", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/ncsi.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Ubuntu/Linux
    portalWebServer->on("/canonical.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/connectivity-check.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Firefox
    portalWebServer->on("/success.txt", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // iOS captive portal success endpoint - signals portal completion
    portalWebServer->on("/connected", []() {
        // Redirect to Apple's success URL to close captive portal
        portalWebServer->sendHeader("Location", "http://captive.apple.com/hotspot-detect.html", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Serve font file from SD card (/fonts/automate_light.ttf)
    portalWebServer->on("/automate_light.ttf", []() {
        if (sdCardMounted) {
            File fontFile = SD.open("/fonts/automate_light.ttf");
            if (fontFile) {
                portalWebServer->sendHeader("Cache-Control", "max-age=86400");
                portalWebServer->streamFile(fontFile, "font/ttf");
                fontFile.close();
                return;
            }
        }
        portalWebServer->send(404, "text/plain", "Font not found");
    });

    portalWebServer->begin();

    portalState = PORTAL_RUNNING;
    drawPortalScreen();
}

void stopCaptivePortal() {
    if (portalDNS != nullptr) {
        portalDNS->stop();
        delete portalDNS;
        portalDNS = nullptr;
    }

    if (portalWebServer != nullptr) {
        portalWebServer->stop();
        delete portalWebServer;
        portalWebServer = nullptr;
    }

    // CRITICAL FIX: Free customPortalHTML memory (can be 10-100KB!)
    customPortalHTML = "";  // String destructor will free memory

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // Restore brightness after low-power portal mode
    M5Cardputer.Display.setBrightness(255);

    portalState = PORTAL_STOPPED;
    portalSSID = "";
}

void handlePortalLoop() {
    static int lastDisplayedCount = 0;

    if (portalState == PORTAL_RUNNING) {
        if (portalDNS != nullptr) {
            portalDNS->processNextRequest();
        }
        if (portalWebServer != nullptr) {
            portalWebServer->handleClient();
        }

        // Update display when visitor count changes
        if (portalVisitorCount != lastDisplayedCount) {
            lastDisplayedCount = portalVisitorCount;
            drawPortalScreen();
        }
    }
}

bool isPortalRunning() {
    return portalState == PORTAL_RUNNING;
}

void drawPortalScreen() {
    // Low power mode - dim display significantly
    M5Cardputer.Display.setBrightness(20);

    canvas.fillScreen(TFT_BLACK);

    // Show SSID in light green
    canvas.setTextSize(1);
    canvas.setTextColor(0x9FE7); // Light green
    String ssidDisplay = portalSSID;
    if (ssidDisplay.length() > 35) {
        ssidDisplay = ssidDisplay.substring(0, 35) + "...";
    }
    int ssidX = 120 - (ssidDisplay.length() * 3);
    canvas.drawString(ssidDisplay.c_str(), ssidX, 15);

    // Show IP in medium green
    IPAddress IP = WiFi.softAPIP();
    String ipStr = IP.toString();
    canvas.setTextColor(0x7FE0); // Medium green
    int ipX = 120 - (ipStr.length() * 3);
    canvas.drawString(ipStr.c_str(), ipX, 25);

    // Centered minimal display
    canvas.setTextSize(2);
    canvas.setTextColor(TFT_GREEN); // Bright green
    canvas.drawString("BROADCASTING", 50, 45);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);

    // Show visitor count - main metric
    canvas.setTextSize(3);
    canvas.setTextColor(TFT_YELLOW);
    String visitors = String(portalVisitorCount);
    int xPos = 120 - (visitors.length() * 9);
    canvas.drawString(visitors.c_str(), xPos, 75);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.drawString("visitors", 95, 105);

    // Instructions
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("` to stop", 90, 122);
  // Push canvas to display
  canvas.pushSprite(0, 0);
}
