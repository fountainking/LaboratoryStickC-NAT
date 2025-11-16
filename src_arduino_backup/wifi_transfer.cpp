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
  <title>Laboratory Transfer</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.typekit.net/wop7tdt.css">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'automate', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      padding: 40px 20px;
      background: #fff;
      color: #000;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    h1 {
      font-size: 32px;
      margin-bottom: 30px;
      text-align: center;
    }
    .upload-box {
      width: 100%;
      max-width: 500px;
      border: 3px dashed #000;
      border-radius: 20px;
      padding: 60px 40px;
      text-align: center;
      cursor: pointer;
      transition: all 0.2s;
      margin-bottom: 20px;
    }
    .upload-box:hover {
      border-color: #666;
      background: #f9f9f9;
    }
    .upload-box.dragover {
      border-color: #000;
      background: #f0f0f0;
    }
    .upload-icon {
      font-size: 48px;
      margin-bottom: 20px;
    }
    .upload-text {
      font-size: 18px;
      margin-bottom: 10px;
    }
    .upload-subtext {
      font-size: 14px;
      color: #666;
      margin-bottom: 20px;
    }
    .btn {
      background: #000;
      color: #fff;
      border: none;
      border-radius: 10px;
      padding: 12px 24px;
      font-size: 16px;
      font-family: 'automate', sans-serif;
      cursor: pointer;
      margin: 10px 5px;
    }
    .btn:hover {
      background: #333;
    }
    .limits {
      max-width: 500px;
      text-align: center;
      margin-top: 20px;
      font-size: 12px;
      color: #666;
    }
    .status {
      margin-top: 20px;
      padding: 15px;
      border-radius: 10px;
      display: none;
      max-width: 500px;
      width: 100%;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
    }
    #fileInput {
      display: none;
    }
  </style>
</head>
<body>
  <h1>Laboratory Transfer</h1>

  <div class="upload-box" id="uploadBox" onclick="document.getElementById('fileInput').click()">
    <div class="upload-icon">⬆</div>
    <div class="upload-text">Drag file here or click to browse</div>
    <div class="upload-subtext">Upload HTML file</div>
    <input type="file" id="fileInput" accept=".html" onchange="handleFile(event)">
  </div>

  <button class="btn" onclick="resetToDefault()">Reset to Default</button>

  <div class="limits">
    <strong>Limits:</strong> HTML 100KB max · Media 2MB max
  </div>

  <div class="status" id="status"></div>

  <script>
    const uploadBox = document.getElementById('uploadBox');
    const fileInput = document.getElementById('fileInput');
    const status = document.getElementById('status');

    // Drag and drop handlers
    uploadBox.addEventListener('dragover', (e) => {
      e.preventDefault();
      uploadBox.classList.add('dragover');
    });

    uploadBox.addEventListener('dragleave', () => {
      uploadBox.classList.remove('dragover');
    });

    uploadBox.addEventListener('drop', (e) => {
      e.preventDefault();
      uploadBox.classList.remove('dragover');
      if (e.dataTransfer.files.length) {
        handleFileUpload(e.dataTransfer.files[0]);
      }
    });

    function handleFile(event) {
      if (event.target.files.length) {
        handleFileUpload(event.target.files[0]);
      }
    }

    async function handleFileUpload(file) {
      if (!file.name.endsWith('.html')) {
        showStatus('Error: Only .html files allowed', 'error');
        return;
      }

      const html = await file.text();
      if (html.length === 0) {
        showStatus('Error: HTML file is empty', 'error');
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
