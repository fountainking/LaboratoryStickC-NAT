#ifndef WIFI_SETUP_PORTAL_H
#define WIFI_SETUP_PORTAL_H

#include <Arduino.h>

const char WIFI_SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Setup - Laboratory</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Arial, sans-serif;
            background: #000;
            color: #fff;
            padding: 20px;
        }
        .container { max-width: 500px; margin: 0 auto; }
        h1 {
            color: #FFE000;
            font-size: 28px;
            margin-bottom: 10px;
            text-align: center;
        }
        .subtitle {
            color: #888;
            text-align: center;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .network-list {
            background: #111;
            border-radius: 8px;
            padding: 10px;
            margin-bottom: 20px;
        }
        .network {
            background: #222;
            border: 2px solid #333;
            border-radius: 6px;
            padding: 15px;
            margin-bottom: 10px;
            cursor: pointer;
            transition: all 0.2s;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network:hover {
            border-color: #FFE000;
            background: #2a2a00;
        }
        .network.selected {
            border-color: #FFE000;
            background: #FFE000;
            color: #000;
        }
        .network-name {
            font-weight: bold;
            font-size: 16px;
        }
        .network-signal {
            font-size: 12px;
            color: #888;
        }
        .network.selected .network-signal {
            color: #333;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #FFE000;
            font-weight: bold;
        }
        input {
            width: 100%;
            padding: 12px;
            background: #222;
            border: 2px solid #333;
            border-radius: 6px;
            color: #fff;
            font-size: 16px;
        }
        input:focus {
            outline: none;
            border-color: #FFE000;
        }
        button {
            width: 100%;
            padding: 15px;
            background: #FFE000;
            color: #000;
            border: none;
            border-radius: 6px;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.2s;
        }
        button:hover {
            background: #fff;
        }
        button:disabled {
            background: #444;
            color: #888;
            cursor: not-allowed;
        }
        .status {
            text-align: center;
            margin-top: 20px;
            padding: 15px;
            border-radius: 6px;
            display: none;
        }
        .status.success {
            background: #004400;
            color: #00ff00;
        }
        .status.error {
            background: #440000;
            color: #ff0000;
        }
        .scanning {
            text-align: center;
            padding: 30px;
            color: #888;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>⭐ WiFi Setup</h1>
        <div class="subtitle">Configure Laboratory WiFi</div>

        <div id="scanning" class="scanning">
            <p>Scanning for networks...</p>
        </div>

        <div id="networkList" class="network-list" style="display:none;"></div>

        <div class="form-group" id="passwordGroup" style="display:none;">
            <label for="password">Password</label>
            <input type="password" id="password" placeholder="Enter WiFi password">
        </div>

        <button id="connectBtn" onclick="connect()" style="display:none;">Connect</button>

        <div id="status" class="status"></div>
    </div>

    <script>
        let selectedSSID = '';

        // Load networks on page load
        window.onload = function() {
            fetch('/scan')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('scanning').style.display = 'none';
                    document.getElementById('networkList').style.display = 'block';

                    const list = document.getElementById('networkList');
                    data.networks.forEach(net => {
                        const div = document.createElement('div');
                        div.className = 'network';
                        div.innerHTML = `
                            <span class="network-name">${net.ssid}</span>
                            <span class="network-signal">${net.rssi} dBm</span>
                        `;
                        div.onclick = () => selectNetwork(net.ssid, div);
                        list.appendChild(div);
                    });
                });
        };

        function selectNetwork(ssid, element) {
            selectedSSID = ssid;

            // Update UI
            document.querySelectorAll('.network').forEach(n => n.classList.remove('selected'));
            element.classList.add('selected');

            document.getElementById('passwordGroup').style.display = 'block';
            document.getElementById('connectBtn').style.display = 'block';
            document.getElementById('password').focus();
        }

        function connect() {
            const password = document.getElementById('password').value;

            if (!selectedSSID) {
                showStatus('Please select a network', 'error');
                return;
            }

            document.getElementById('connectBtn').disabled = true;
            showStatus('Connecting...', 'success');

            fetch('/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid: selectedSSID, password: password })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    showStatus('CONNECTED! Redirecting...', 'success');
                    setTimeout(() => {
                        window.location.href = data.redirect;
                    }, 1000);
                } else {
                    showStatus('Failed: ' + data.message, 'error');
                    document.getElementById('connectBtn').disabled = false;
                }
            })
            .catch(e => {
                showStatus('Connection error', 'error');
                document.getElementById('connectBtn').disabled = false;
            });
        }

        function showStatus(message, type) {
            const status = document.getElementById('status');
            status.textContent = message;
            status.className = 'status ' + type;
            status.style.display = 'block';
        }
    </script>
</body>
</html>
)rawliteral";

#endif
