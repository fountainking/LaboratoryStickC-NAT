#include "captive_portal.h"
#include <esp_wifi.h>

// NOTE: NAT (Network Address Translation) is NOT available in Arduino ESP32 v2.0.14
// The SDK was not compiled with CONFIG_LWIP_IPV4_NAPT=y
// See NAT_STATUS.md for details and alternatives

// Portal globals
PortalState portalState = PORTAL_STOPPED;
WebServer* portalWebServer = nullptr;
DNSServer* portalDNS = nullptr;
int portalVisitorCount = 0;
String portalSSID = "";
String customPortalHTML = "";
unsigned long portalStartTime = 0;
bool internetUnlocked = false;  // Global flag for DNS behavior

// Unique visitor tracking
#define MAX_VISITORS 50
IPAddress visitorIPs[MAX_VISITORS];
int uniqueVisitorCount = 0;

// MAC address whitelist for authenticated clients
#define MAX_AUTHENTICATED 50
String authenticatedMACs[MAX_AUTHENTICATED];
int authenticatedCount = 0;

bool isNewVisitor(IPAddress ip) {
    // Skip local/invalid IPs
    if (ip[0] == 0) return false;

    for (int i = 0; i < uniqueVisitorCount; i++) {
        if (visitorIPs[i] == ip) return false;
    }

    if (uniqueVisitorCount < MAX_VISITORS) {
        visitorIPs[uniqueVisitorCount++] = ip;
        portalVisitorCount++;
        Serial.println("[Portal] New visitor: " + ip.toString() + " (total: " + String(portalVisitorCount) + ")");
        return true;
    }
    return false;
}

// Check if client MAC is authenticated
bool isClientAuthenticated(const String& macAddress) {
    for (int i = 0; i < authenticatedCount; i++) {
        if (authenticatedMACs[i].equalsIgnoreCase(macAddress)) {
            return true;
        }
    }
    return false;
}

// Add MAC to authenticated list
void authenticateClient(const String& macAddress) {
    if (!isClientAuthenticated(macAddress) && authenticatedCount < MAX_AUTHENTICATED) {
        authenticatedMACs[authenticatedCount++] = macAddress;
        Serial.println("[Portal] Authenticated MAC: " + macAddress + " (total: " + String(authenticatedCount) + ")");
    }
}

// Get client MAC address from IP
String getClientMAC(IPAddress ip) {
    wifi_sta_list_t stationList;
    esp_wifi_ap_get_sta_list(&stationList);

    for (int i = 0; i < stationList.num; i++) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                 stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                 stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
        // Note: ESP32 doesn't easily map IP to MAC, so we'll use client header
        return String(macStr);
    }
    return "";
}

void startCaptivePortal(const String& ssid, const String& html) {
    stopCaptivePortal(); // Stop any existing portal

    portalSSID = ssid;
    portalVisitorCount = 0;
    portalStartTime = millis();
    customPortalHTML = html;

    // Configure Access Point with custom IP configuration
    // If already connected to WiFi, use STA+AP mode for internet-capable portal!
    bool hasWiFi = (WiFi.status() == WL_CONNECTED);
    String savedWifiSSID = "";
    String savedWifiPass = "";

    if (hasWiFi) {
        Serial.println("[Portal] WiFi connected - enabling internet-capable portal (STA+AP mode)");
        Serial.printf("[Portal] Current IP: %s\n", WiFi.localIP().toString().c_str());

        // Save current WiFi credentials before mode switch (mode switch disconnects!)
        savedWifiSSID = WiFi.SSID();
        savedWifiPass = WiFi.psk();

        // Switch to STA+AP mode (this will disconnect WiFi temporarily)
        WiFi.mode(WIFI_AP_STA);
        delay(100);

        // Reconnect to WiFi using saved credentials
        Serial.printf("[Portal] Reconnecting to WiFi: %s\n", savedWifiSSID.c_str());
        WiFi.begin(savedWifiSSID.c_str(), savedWifiPass.c_str());

        // Wait for reconnection (up to 10 seconds)
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[Portal] WiFi RECONNECTED! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.println("[Portal] WiFi bridge mode ACTIVE - clients will have internet access");
        } else {
            Serial.println("[Portal] WARNING: WiFi reconnection FAILED - bridge unavailable");
        }
    } else {
        Serial.println("[Portal] No WiFi - standard portal (AP only)");
        WiFi.mode(WIFI_AP);
    }

    // Set custom AP IP configuration for better captive portal detection
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    // Configure AP IP (note: ESP32 Arduino doesn't support DNS config in softAPConfig)
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("[Portal] ERROR: Failed to configure AP");
    } else {
        Serial.println("[Portal] AP configured - gateway: 192.168.4.1");
    }

    WiFi.softAP(ssid.c_str());

    delay(100);

    IPAddress IP = WiFi.softAPIP();
    Serial.printf("[Portal] AP IP: %s\n", IP.toString().c_str());

    // WARNING: NAT is not available in this SDK build
    // Clients will see captive portal but won't get internet after authentication
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Portal] ⚠ WARNING: NAT not available in this SDK");
        Serial.println("[Portal] Captive portal will work, but internet sharing requires:");
        Serial.println("[Portal]  1. ESP-IDF framework (not Arduino), OR");
        Serial.println("[Portal]  2. Custom SDK rebuild with CONFIG_LWIP_IPV4_NAPT=y");
        Serial.println("[Portal] See NAT_STATUS.md for details");
    }

    // Always start DNS server for captive portal detection
    // It will hijack ALL requests to trigger the portal popup
    portalDNS = new DNSServer();
    portalDNS->setTTL(0);  // Set TTL to 0 so clients re-query after portal
    portalDNS->start(53, "*", IP);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Portal] DNS active for captive detection - will stop after authentication");
    } else {
        Serial.println("[Portal] DNS hijacking ACTIVE (no upstream internet)");
    }

    // Start web server
    portalWebServer = new WebServer(80);

    // Handle all requests - check authentication first
    portalWebServer->onNotFound([]() {
        IPAddress clientIP = portalWebServer->client().remoteIP();

        // Check if client is authenticated
        wifi_sta_list_t stationList;
        esp_wifi_ap_get_sta_list(&stationList);

        bool authenticated = false;
        for (int i = 0; i < stationList.num; i++) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                     stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                     stationList.sta[i].mac[4], stationList.sta[i].mac[5]);

            if (isClientAuthenticated(String(macStr))) {
                authenticated = true;
                break;
            }
        }

        if (authenticated && WiFi.status() == WL_CONNECTED) {
            // Authenticated - return 404 so browser uses real DNS
            portalWebServer->send(404, "text/plain", "Not Found");
        } else {
            // Not authenticated - show portal
            isNewVisitor(clientIP);
            portalWebServer->send(200, "text/html", customPortalHTML);
        }
    });

    // Root handler
    portalWebServer->on("/", []() {
        IPAddress clientIP = portalWebServer->client().remoteIP();

        // Check if client is authenticated
        wifi_sta_list_t stationList;
        esp_wifi_ap_get_sta_list(&stationList);

        bool authenticated = false;
        for (int i = 0; i < stationList.num; i++) {
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                     stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                     stationList.sta[i].mac[4], stationList.sta[i].mac[5]);

            if (isClientAuthenticated(String(macStr))) {
                authenticated = true;
                break;
            }
        }

        if (authenticated && WiFi.status() == WL_CONNECTED) {
            // Authenticated - return 404 so browser uses real DNS
            portalWebServer->send(404, "text/plain", "Not Found");
        } else {
            // Not authenticated - show portal
            isNewVisitor(clientIP);
            portalWebServer->send(200, "text/html", customPortalHTML);
        }
    });

    // Captive portal detection endpoints
    // Android - return 200 with HTML to trigger portal popup
    portalWebServer->on("/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    portalWebServer->on("/gen_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    // Additional Android connectivity checks
    portalWebServer->on("/connectivitycheck.gstatic.com/generate_204", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    // iOS/Apple - return 200 with HTML (not redirect!)
    portalWebServer->on("/hotspot-detect.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    portalWebServer->on("/library/test/success.html", []() {
        isNewVisitor(portalWebServer->client().remoteIP());
        portalWebServer->send(200, "text/html", customPortalHTML);
    });

    // Windows - special case: redirect to logout.net
    portalWebServer->on("/connecttest.txt", []() {
        portalVisitorCount++;
        portalWebServer->sendHeader("Location", "http://logout.net", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/ncsi.txt", []() {
        portalVisitorCount++;
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Ubuntu/Linux
    portalWebServer->on("/canonical.html", []() {
        portalVisitorCount++;
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    portalWebServer->on("/connectivity-check.html", []() {
        portalVisitorCount++;
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Firefox
    portalWebServer->on("/success.txt", []() {
        portalVisitorCount++;
        portalWebServer->sendHeader("Location", "http://192.168.4.1", true);
        portalWebServer->send(302, "text/plain", "");
    });

    // Grant internet access endpoint (enables NAT forwarding for client)
    // GET for status check only
    portalWebServer->on("/grant_access", HTTP_GET, []() {
        StaticJsonDocument<256> resp;
        resp["success"] = (WiFi.status() == WL_CONNECTED);
        String rs;
        serializeJson(resp, rs);
        portalWebServer->send(200, "application/json", rs);
    });

    // POST to activate access - STOP DNS to enable internet
    portalWebServer->on("/grant_access", HTTP_POST, []() {
        StaticJsonDocument<256> response;
        if (WiFi.status() == WL_CONNECTED) {
            IPAddress clientIP = portalWebServer->client().remoteIP();

            // STOP DNS server completely - this enables real internet
            if (portalDNS != nullptr) {
                Serial.println("[Portal] STOPPING DNS - clients will now use real DNS (8.8.8.8)");
                portalDNS->stop();
                delete portalDNS;
                portalDNS = nullptr;
            }

            response["success"] = true;
            internetUnlocked = true;
            Serial.println("[Portal] Access granted! DNS hijacking stopped");
            Serial.println("[Portal] ⚠ WARNING: Internet NOT available (NAT unsupported in this SDK)");
            Serial.println("[Portal] User will see 'No Internet' - this is expected");
        } else {
            response["success"] = false;
        }
        String responseStr;
        serializeJson(response, responseStr);
        portalWebServer->send(200, "application/json", responseStr);
    });

    // Success page - honest about limitations
    portalWebServer->on("/success", []() {
        portalWebServer->send(200, "text/html", "<!DOCTYPE html><html><head><title>Portal Complete</title><style>*{margin:0;padding:0;}body{font-family:sans-serif;background:#000;color:#fff;display:flex;align-items:center;justify-content:center;min-height:100vh;text-align:center;padding:20px;}.icon{font-size:60px;margin-bottom:15px;}h1{color:#FFE000;margin-bottom:15px;font-size:1.5em;}p{font-size:0.95em;line-height:1.5;max-width:380px;margin:10px auto;}.warning{color:#ff6b6b;margin-top:15px;font-size:0.85em;}.code{background:#222;padding:8px 15px;border-radius:5px;color:#0f0;font-family:monospace;margin:10px 0;display:inline-block;}a{color:#FFE000;text-decoration:underline;}</style></head><body><div><div class='icon'>⚠️</div><h1>AUTHENTICATION COMPLETE</h1><p>You've accessed the Laboratory portal.</p><div class='warning'><p><strong>Internet Not Available</strong></p><p>This ESP32 build doesn't support NAT.</p><p>Portal works for demonstration only.</p></div><p style='margin-top:20px;font-size:0.9em;'><a href='https://laboratory.mx' target='_blank'>Visit Laboratory.mx</a></p></div></body></html>");
    });

    portalWebServer->begin();

    portalState = PORTAL_RUNNING;
    Serial.println("[Portal] Started: " + ssid);
    Serial.println("[Portal] IP: " + IP.toString());
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

    // Free customPortalHTML memory (can be 10-100KB!)
    customPortalHTML = "";

    // Reset visitor tracking
    uniqueVisitorCount = 0;
    portalVisitorCount = 0;

    // Reset authentication whitelist
    authenticatedCount = 0;

    // Disconnect AP but preserve STA connection if it exists
    WiFi.softAPdisconnect(true);
    if (WiFi.status() == WL_CONNECTED) {
        // Keep STA mode active (WiFi connection)
        WiFi.mode(WIFI_STA);
        Serial.println("[Portal] Stopped - WiFi connection preserved");
    } else {
        // No WiFi connection, turn off completely
        WiFi.mode(WIFI_OFF);
        Serial.println("[Portal] Stopped");
    }
    delay(100);

    portalState = PORTAL_STOPPED;
    portalSSID = "";
}

void handlePortalLoop() {
    if (portalState == PORTAL_RUNNING) {
        if (portalDNS != nullptr) {
            portalDNS->processNextRequest();
        }
        if (portalWebServer != nullptr) {
            portalWebServer->handleClient();
        }
    }
}

bool isPortalRunning() {
    return portalState == PORTAL_RUNNING;
}

int getPortalVisitorCount() {
    return portalVisitorCount;
}

String getPortalSSID() {
    return portalSSID;
}

IPAddress getPortalIP() {
    return WiFi.softAPIP();
}
