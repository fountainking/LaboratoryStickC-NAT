#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Portal state
enum PortalState {
  PORTAL_STOPPED,
  PORTAL_RUNNING
};

// Portal functions
void startCaptivePortal(const String& ssid, const String& html);
void stopCaptivePortal();
void handlePortalLoop();
bool isPortalRunning();
int getPortalVisitorCount();
String getPortalSSID();
IPAddress getPortalIP();

// Portal globals
extern PortalState portalState;
extern WebServer* portalWebServer;
extern DNSServer* portalDNS;
extern int portalVisitorCount;
extern String portalSSID;

#endif
