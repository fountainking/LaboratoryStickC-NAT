#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>

#define FIRMWARE_VERSION "1.0.0"
#define GITHUB_API_URL "https://api.github.com/repos/fountainking/LaboratoryStickC/releases/latest"

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
