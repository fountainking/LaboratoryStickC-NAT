#include "captive_portal.h"

// Portal globals
PortalState portalState = PORTAL_STOPPED;
WebServer* portalWebServer = nullptr;
DNSServer* portalDNS = nullptr;
int portalVisitorCount = 0;
String portalSSID = "";
String customPortalHTML = "";
unsigned long portalStartTime = 0;

// Unique visitor tracking
#define MAX_VISITORS 50
IPAddress visitorIPs[MAX_VISITORS];
int uniqueVisitorCount = 0;

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

void startCaptivePortal(const String& ssid, const String& html) {
    stopCaptivePortal(); // Stop any existing portal

    portalSSID = ssid;
    portalVisitorCount = 0;
    portalStartTime = millis();
    customPortalHTML = html;

    // Configure Access Point with custom IP configuration
    WiFi.mode(WIFI_AP);

    // Set custom AP IP configuration for better captive portal detection
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);

    WiFi.softAP(ssid.c_str());

    delay(100);

    IPAddress IP = WiFi.softAPIP();

    // Start DNS server (captures all DNS requests)
    portalDNS = new DNSServer();
    portalDNS->setTTL(3600);
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

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    portalState = PORTAL_STOPPED;
    portalSSID = "";

    Serial.println("[Portal] Stopped");
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
