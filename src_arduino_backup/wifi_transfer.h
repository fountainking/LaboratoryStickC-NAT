#ifndef WIFI_TRANSFER_H
#define WIFI_TRANSFER_H

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Preferences.h>

// WiFi Transfer state
enum WiFiTransferState {
  TRANSFER_STOPPED,
  TRANSFER_RUNNING
};

// Transfer functions
void startTransferServer();
void stopTransferServer();
void handleTransferLoop();
bool isTransferRunning();
String getStoredPortalHTML();
void savePortalHTML(const String& html);

// Transfer globals
extern WiFiTransferState transferState;
extern WebServer* transferWebServer;
extern int uploadCount;

#endif
