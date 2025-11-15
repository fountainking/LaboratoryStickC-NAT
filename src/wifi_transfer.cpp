#include "wifi_transfer.h"
#include "portal_content.h"

WiFiTransferState transferState = TRANSFER_STOPPED;
WebServer* transferWebServer = nullptr;
int uploadCount = 0;

// Preferences object for storing custom HTML
Preferences preferences;

// Simplified HTML for upload interface (StickC has no SD card - stores HTML in NVS)
const char TRANSFER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Laboratory Portal Upload</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Courier New', monospace;
      padding: 20px;
      background: #000;
      color: #ffd700;
      min-height: 100vh;
    }
    .header {
      background: #ffd700;
      color: #000;
      border: 3px solid #ff0000;
      border-radius: 30px;
      padding: 15px 20px;
      margin: 10px 0 20px 0;
      text-align: center;
      font-size: 24px;
      font-weight: bold;
      letter-spacing: 2px;
    }
    .upload-section {
      background: #222;
      border: 3px solid #ffd700;
      border-radius: 25px;
      padding: 30px;
      margin-bottom: 20px;
    }
    .upload-section h2 {
      color: #ff0000;
      font-size: 24px;
      margin-bottom: 15px;
    }
    .upload-section p {
      color: #ffd700;
      font-size: 14px;
      margin: 10px 0;
    }
    textarea {
      width: 100%;
      min-height: 300px;
      background: #000;
      color: #0f0;
      border: 2px solid #ffd700;
      border-radius: 10px;
      padding: 15px;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      margin: 15px 0;
    }
    .btn {
      background: #ffd700;
      color: #000;
      border: 2px solid #ff0000;
      border-radius: 20px;
      padding: 12px 24px;
      font-size: 16px;
      font-weight: bold;
      font-family: 'Courier New', monospace;
      cursor: pointer;
      margin: 8px 4px;
    }
    .btn:hover {
      background: #ff0000;
      color: #fff;
    }
    .btn-danger {
      background: #ff0000;
      color: #fff;
      border-color: #ffd700;
    }
    .btn-danger:hover {
      background: #cc0000;
    }
    .status {
      background: #222;
      border: 2px solid #ffd700;
      border-radius: 15px;
      padding: 15px;
      margin-top: 20px;
      color: #0f0;
      font-weight: bold;
      display: none;
    }
    .current-content {
      background: #222;
      border: 3px solid #ff0000;
      border-radius: 25px;
      padding: 20px;
      margin-top: 20px;
    }
    .current-content h3 {
      color: #ff0000;
      margin-bottom: 10px;
    }
  </style>
</head>
<body>
  <div class="header">LABORATORY PORTAL</div>

  <div class="upload-section">
    <h2>Upload Custom Portal HTML</h2>
    <p>Paste your HTML below and upload. This will replace the default Laboratory portal.</p>
    <p><strong>Note:</strong> StickC has limited storage (~32KB for HTML). Keep it minimal!</p>

    <textarea id="htmlInput" placeholder="Paste your custom HTML here..."></textarea>

    <button class="btn" onclick="uploadHTML()">Upload Portal HTML</button>
    <button class="btn" onclick="loadDefault()">Load Default Laboratory HTML</button>
    <button class="btn btn-danger" onclick="resetToDefault()">Reset to Default</button>
  </div>

  <div class="current-content">
    <h3>Current Portal Size</h3>
    <p id="portalSize">Loading...</p>
  </div>

  <div class="status" id="status">Ready</div>

  <script>
    function showStatus(msg) {
      const status = document.getElementById('status');
      status.textContent = msg;
      status.style.display = 'block';
      setTimeout(() => status.style.display = 'none', 3000);
    }

    async function uploadHTML() {
      const html = document.getElementById('htmlInput').value;
      if (html.length === 0) {
        showStatus('Error: HTML cannot be empty');
        return;
      }

      if (html.length > 32000) {
        showStatus('Warning: HTML is very large (' + html.length + ' bytes). May not fit!');
      }

      try {
        const response = await fetch('/upload', {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain' },
          body: html
        });

        if (response.ok) {
          showStatus('Success! Portal HTML updated (' + html.length + ' bytes)');
          updateSize();
        } else {
          showStatus('Error: Upload failed');
        }
      } catch (error) {
        showStatus('Error: ' + error.message);
      }
    }

    async function resetToDefault() {
      if (!confirm('Reset to default Laboratory portal?')) return;

      try {
        const response = await fetch('/reset', { method: 'POST' });
        if (response.ok) {
          showStatus('Reset to default Laboratory portal');
          document.getElementById('htmlInput').value = '';
          updateSize();
        }
      } catch (error) {
        showStatus('Error: ' + error.message);
      }
    }

    async function loadDefault() {
      try {
        const response = await fetch('/default');
        const html = await response.text();
        document.getElementById('htmlInput').value = html;
        showStatus('Default Laboratory HTML loaded into editor');
      } catch (error) {
        showStatus('Error loading default HTML');
      }
    }

    async function updateSize() {
      try {
        const response = await fetch('/size');
        const data = await response.json();
        document.getElementById('portalSize').textContent =
          data.size + ' bytes' + (data.custom ? ' (custom)' : ' (default)');
      } catch (error) {
        document.getElementById('portalSize').textContent = 'Error loading size';
      }
    }

    updateSize();
  </script>
</body>
</html>
)rawliteral";

void handleUpload() {
    if (transferWebServer->hasArg("plain")) {
        String html = transferWebServer->arg("plain");

        if (html.length() > 32000) {
            transferWebServer->send(413, "text/plain", "HTML too large");
            return;
        }

        savePortalHTML(html);
        uploadCount++;
        transferWebServer->send(200, "text/plain", "OK");
        Serial.println("[Transfer] Portal HTML uploaded: " + String(html.length()) + " bytes");
    } else {
        transferWebServer->send(400, "text/plain", "No HTML data");
    }
}

void handleReset() {
    preferences.begin("portal", false);
    preferences.remove("customHTML");
    preferences.end();
    transferWebServer->send(200, "text/plain", "Reset to default");
    Serial.println("[Transfer] Reset to default portal");
}

void handleDefault() {
    transferWebServer->send_P(200, "text/html", LABORATORY_HTML);
}

void handleSize() {
    preferences.begin("portal", true);
    String customHTML = preferences.getString("customHTML", "");
    preferences.end();

    String json = "{";
    json += "\"size\":" + String(customHTML.length() > 0 ? customHTML.length() : strlen_P(LABORATORY_HTML)) + ",";
    json += "\"custom\":" + String(customHTML.length() > 0 ? "true" : "false");
    json += "}";

    transferWebServer->send(200, "application/json", json);
}

void startTransferServer() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Transfer] Error: WiFi not connected");
        return;
    }

    transferWebServer = new WebServer(80);

    transferWebServer->on("/", HTTP_GET, []() {
        transferWebServer->send_P(200, "text/html", TRANSFER_HTML);
    });

    transferWebServer->on("/upload", HTTP_POST, handleUpload);
    transferWebServer->on("/reset", HTTP_POST, handleReset);
    transferWebServer->on("/default", HTTP_GET, handleDefault);
    transferWebServer->on("/size", HTTP_GET, handleSize);

    transferWebServer->begin();
    transferState = TRANSFER_RUNNING;
    uploadCount = 0;

    Serial.println("[Transfer] Server started: http://" + WiFi.localIP().toString());
}

void stopTransferServer() {
    if (transferWebServer != nullptr) {
        transferWebServer->stop();
        delete transferWebServer;
        transferWebServer = nullptr;
    }

    transferState = TRANSFER_STOPPED;
    Serial.println("[Transfer] Server stopped");
}

void handleTransferLoop() {
    if (transferState == TRANSFER_RUNNING && transferWebServer != nullptr) {
        transferWebServer->handleClient();
    }
}

bool isTransferRunning() {
    return transferState == TRANSFER_RUNNING;
}

String getStoredPortalHTML() {
    preferences.begin("portal", true);
    String customHTML = preferences.getString("customHTML", "");
    preferences.end();

    if (customHTML.length() > 0) {
        Serial.println("[Transfer] Using custom portal HTML: " + String(customHTML.length()) + " bytes");
        return customHTML;
    } else {
        Serial.println("[Transfer] Using default Laboratory HTML");
        return String(FPSTR(LABORATORY_HTML));
    }
}

void savePortalHTML(const String& html) {
    preferences.begin("portal", false);
    preferences.putString("customHTML", html);
    preferences.end();
    Serial.println("[Transfer] Saved custom portal HTML: " + String(html.length()) + " bytes");
}
