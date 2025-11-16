#include "portal_content.h"

// Laboratory default portal HTML with embedded font
// Borrowed from LaboratoryM5 portal_manager.cpp:19-195
const char LABORATORY_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Laboratory</title>
    <link rel="stylesheet" href="https://use.typekit.net/wop7tdt.css">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: "automate", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            margin: 0;
            padding: 30px 20px;
            background: #f5f5f5;
            color: #000;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: flex-start;
        }
        .frame {
            max-width: 600px;
            width: 100%;
            background: #fff;
            border: 3px solid #000;
            border-radius: 40px;
            padding: 35px 40px;
            margin-bottom: 30px;
        }
        .logo-container {
            max-width: 600px;
            width: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
            margin-bottom: 30px;
        }
        .logo-container svg {
            width: 100%;
            max-width: 400px;
            height: auto;
        }
        .description {
            text-align: center;
            line-height: 1.7;
            font-size: 1.05em;
            color: #000;
        }
        .recruiting-section {
            max-width: 600px;
            width: 100%;
            text-align: center;
            margin-bottom: 30px;
        }
        .recruiting {
            text-align: center;
            margin: 0 0 20px 0;
            font-size: 1.05em;
            color: #000;
            line-height: 1.6;
        }
        .recruiting strong {
            font-weight: normal;
        }
        strong, b {
            font-weight: normal;
        }
        body {
            font-synthesis: none;
        }
        .cta-button {
            display: block;
            background: #fff;
            color: #000;
            border: 3px solid #000;
            border-radius: 50px;
            padding: 15px 40px;
            font-size: 1.1em;
            font-weight: bold;
            text-align: center;
            margin: 0 auto;
            max-width: 350px;
            cursor: pointer;
            text-decoration: none;
            font-family: "automate", sans-serif;
        }
        .cta-button:hover {
            background: #f5f5f5;
        }
        .video-section {
            max-width: 600px;
            width: 100%;
            margin-bottom: 30px;
        }
        .video-title {
            text-align: center;
            font-size: 1.1em;
            margin-bottom: 20px;
            font-weight: normal;
            color: #000;
        }
        .video-frame {
            width: 100%;
            background: #fff;
            border: 3px solid #000;
            border-radius: 40px;
            padding: 30px;
            text-align: center;
        }
        .video-frame a {
            display: inline-block;
            background: #000;
            color: #fff;
            padding: 15px 30px;
            border-radius: 10px;
            text-decoration: none;
            font-weight: bold;
            font-size: 1.1em;
        }
        .video-frame a:hover {
            background: #333;
        }
    </style>
</head>
<body>
    <div class="logo-container">
        <svg viewBox="0 0 379 96.1" xmlns="http://www.w3.org/2000/svg">
                <defs><style>.st0{fill:#f5c429;stroke-width:4px}.st0,.st1{stroke:#000;stroke-miterlimit:10}.st2{fill:#fff}</style></defs>
                <g><rect class="st2" x="6" y="6" width="367" height="84" rx="42" ry="42"/>
                <path d="M12,48c0-19.9,16.4-35.8,36.1-36s8.7,0,13.1,0c24.2,0,48.5,0,72.7,0,32.8,0,65.6,0,98.4,0,26.3,0,52.6,0,79,0s12.1,0,18.1,0c14.4,0,27.5,7.1,34,20.3,9.9,20-1.8,44.9-23.3,50.5-5.6,1.5-11.5,1.2-17.2,1.2h-66.9c-32.4,0-64.8,0-97.2,0s-55.9,0-83.8,0c-16.4,0-36.6,3-50-8.4-8.2-6.9-12.7-16.9-12.9-27.6S-.1,40.3,0,48c.4,24.5,18.7,45.1,43.2,47.7s10.2.3,15.2.3c24.7,0,49.4,0,74.1,0h102.7c26.7,0,53.3,0,80,0,5.2,0,10.3,0,15.5,0,20.4-.2,38.4-12.7,45.5-31.9,8.2-22.2-2.3-48.3-23.5-58.9S334.8,0,325.2,0h-67c-34,0-67.9,0-101.9,0h-85.9c-6.8,0-13.6,0-20.5,0-15,0-29.3,5.6-39,17.4S0,36.9,0,48s12,7.7,12,0Z"/></g>
                <g><g><path class="st1" d="M47.8,59.8c1,0,1.4.8,1.4,1.7s-.5,1.7-1.4,1.7h-16.1v-28.3c0-1,.9-1.4,1.9-1.4s1.9.5,1.9,1.4v24.8h12.4Z"/>
                <path class="st1" d="M75.6,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8.5h-14v8.5c0,1-.8,1.5-1.8,1.5s-1.8-.5-1.8-1.5v-16.5c0-2.2.5-3.9,2.2-5.6l5.9-5.8c.5-.5,1.3-.9,2.5-.9s2,.5,2.5.9l6,5.8c1.7,1.6,2.2,3.3,2.2,5.6v16.5ZM72,50.4v-4.6c0-1.5-.3-2.5-1.4-3.5l-5.7-5.5-5.7,5.5c-1.1,1.1-1.4,2-1.4,3.5v4.6h14.1Z"/>
                <path class="st1" d="M103.2,56.2c0,4.4-2.5,7.2-7.4,7.2h-12.9v-29.5h12.9c4.4,0,7.3,3,7.3,7.2v2.1c0,3-1.2,4.5-2.7,5.3,1.6.7,2.8,2.7,2.8,5v2.8ZM96.3,46.8c2.2,0,3.1-.8,3.1-3.7v-2.4c0-2.2-1.4-3.5-3.4-3.5h-9.6v9.6h9.9ZM95.7,60.2c2.4,0,3.8-1.2,3.8-3.5v-2.7c0-2.7-1.1-3.8-3-3.8h-10.2v10.1h9.3Z"/>
                <path d="M131.9,56.3c0,4-3.2,7.3-7.6,7.3h-6.8c-4.4,0-7.6-3.2-7.6-7.3v-15.3c0-4.1,3.4-7.3,7.4-7.3h7.1c4.2,0,7.5,3.2,7.5,7.3v15.3ZM124.7,60.1c2.2,0,3.7-1.7,3.7-3.5v-15.9c0-1.9-1.6-3.5-3.5-3.5h-7.8c-2.1,0-3.5,1.6-3.5,3.5v15.9c0,1.9,1.5,3.5,3.7,3.5h7.5Z"/>
                <path d="M159.6,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8c0-2.4-1.3-3.7-3.5-3.7h-9.6v11.7c0,1-.8,1.5-1.7,1.5s-1.8-.5-1.8-1.5v-28.4h13.2c4.1,0,7.1,2.9,7.1,7v2.8c0,2.9-1.4,4.9-3.1,5.7,1.7.6,3.1,2.5,3.1,4.7v8.3ZM152.8,47.1c2.2,0,3.3-1.2,3.3-3.3v-3.5c0-2-1.3-3.2-3.2-3.2h-10.1v10h10Z"/>
                <path d="M187.7,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8.5h-14v8.5c0,1-.8,1.5-1.8,1.5s-1.8-.5-1.8-1.5v-16.5c0-2.2.5-3.9,2.2-5.6l5.9-5.8c.5-.5,1.3-.9,2.5-.9s2,.5,2.5.9l6,5.8c1.7,1.6,2.2,3.3,2.2,5.6v16.5ZM184.1,50.4v-4.6c0-1.5-.3-2.5-1.4-3.5l-5.7-5.5-5.7,5.5c-1.1,1.1-1.4,2-1.4,3.5v4.6h14.1Z"/>
                <path d="M205.8,62.1c0,1-.9,1.5-1.9,1.5s-2-.5-2-1.5v-24.7h-7.8c-1,0-1.4-.7-1.4-1.7s.5-1.8,1.4-1.8h19.5c1,0,1.5.8,1.5,1.8s-.5,1.7-1.5,1.7h-7.8v24.7Z"/>
                <path d="M241.5,56.3c0,4-3.2,7.3-7.6,7.3h-6.8c-4.4,0-7.6-3.2-7.6-7.3v-15.3c0-4.1,3.4-7.3,7.4-7.3h7.1c4.2,0,7.5,3.2,7.5,7.3v15.3ZM234.3,60.1c2.2,0,3.7-1.7,3.7-3.5v-15.9c0-1.9-1.6-3.5-3.5-3.5h-7.8c-2.1,0-3.5,1.6-3.5,3.5v15.9c0,1.9,1.5,3.5,3.7,3.5h7.5Z"/>
                <path d="M269.2,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-8c0-2.4-1.3-3.7-3.5-3.7h-9.6v11.7c0,1-.8,1.5-1.7,1.5s-1.8-.5-1.8-1.5v-28.4h13.2c4.1,0,7.1,2.9,7.1,7v2.8c0,2.9-1.3,4.9-3.1,5.7,1.7.6,3.1,2.5,3.1,4.7v8.3ZM262.4,47.1c2.2,0,3.3-1.2,3.3-3.3v-3.5c0-2-1.3-3.2-3.2-3.2h-10.1v10h10Z"/>
                <path d="M287.7,62.1c0,1-.8,1.5-1.8,1.5s-1.9-.5-1.9-1.5v-9.9l-7-6.4c-1.1-1.1-1.7-2.6-1.7-4.5v-6.3c0-1,.8-1.4,1.8-1.4s1.8.5,1.8,1.4v6.1c0,1.2,0,1.9.8,2.5l5.7,5.2h.7l5.7-5.2c.7-.6.9-1.3.9-2.5v-6.1c0-1,.8-1.4,1.8-1.4s1.8.5,1.8,1.4v6.3c0,2-.3,3.2-1.7,4.5l-7,6.4v9.9Z"/></g>
                <path class="st0" d="M330.7,26.9l5,10.2c.4.8,1.2,1.4,2,1.5l11.2,1.6c2.2.3,3.1,3.1,1.5,4.6l-8.1,7.9c-.6.6-.9,1.5-.8,2.4l1.9,11.2c.4,2.2-1.9,3.9-3.9,2.9l-10-5.3c-.8-.4-1.7-.4-2.5,0l-10,5.3c-2,1-4.3-.6-3.9-2.9l1.9-11.2c.2-.9-.1-1.8-.8-2.4l-8.1-7.9c-1.6-1.6-.7-4.3,1.5-4.6l11.2-1.6c.9-.1,1.6-.7,2-1.5l5-10.2c1-2,3.9-2,4.9,0Z"/></g>
        </svg>
    </div>

    <div class="frame">
        <div class="description">
            <strong>Laboratory</strong> is a workforce economic program centered
            around entrepreneurship that offers physical classrooms, retail store fronts,
            and content production studios designed to improve economic outcomes by
            addressing the skills, training, and opportunities individuals need to succeed.
        </div>
    </div>

    <div class="recruiting-section">
        <div class="recruiting">
            We're recruiting for our<br>
            <strong>*EXPERT COMMITTEE*</strong><br>
            If you know anything about<br>
            being an entreprenuer:
        </div>
        <a href="mailto:info@laboratory.mx?subject=Expert Committee Inquiry" class="cta-button">CONNECT WITH US!</a>
    </div>

    <div class="video-section">
        <div class="video-title">Get Connected</div>
        <div class="video-frame">
            <div id="internetStatus" style="display: none; margin-bottom: 20px; padding: 15px; background: #fff3cd; border: 2px solid #ffc107; border-radius: 10px; color: #856404;">
                ⚠️ Device not connected to internet - WiFi bridge unavailable
            </div>
            <button id="connectBtn" onclick="connectToWiFi()" style="display: inline-block; background: #000; color: #fff; padding: 15px 30px; border-radius: 10px; border: none; font-weight: bold; font-size: 1.1em; cursor: pointer; font-family: 'automate', sans-serif;">Connect to WiFi</button>
            <p id="status" style="margin-top: 15px; font-size: 0.9em; color: #666;">Click to enable internet access</p>
        </div>
    </div>

    <script>
        // Check internet status on load
        window.onload = function() {
            fetch('/grant_access', {
                method: 'GET'
            })
            .then(r => r.json())
            .then(data => {
                if (!data.success) {
                    document.getElementById('internetStatus').style.display = 'block';
                    document.getElementById('connectBtn').disabled = true;
                    document.getElementById('connectBtn').style.background = '#666';
                    document.getElementById('connectBtn').style.cursor = 'not-allowed';
                    document.getElementById('status').textContent = 'Internet unavailable - device offline';
                }
            })
            .catch(e => {
                console.log('Status check failed:', e);
            });
        };

        function connectToWiFi() {
            const btn = document.getElementById('connectBtn');
            const status = document.getElementById('status');

            btn.disabled = true;
            btn.style.background = '#666';
            btn.textContent = 'Connecting...';
            status.textContent = 'Enabling internet access...';

            fetch('/grant_access', {
                method: 'POST'
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    // Redirect to success page
                    window.location.href = '/success';
                } else {
                    btn.style.background = '#AA0000';
                    btn.textContent = 'Failed';
                    status.textContent = 'Connection failed. Try again.';
                    btn.disabled = false;
                }
            })
            .catch(e => {
                btn.style.background = '#AA0000';
                btn.textContent = 'Error';
                status.textContent = 'Network error. Try again.';
                btn.disabled = false;
            });
        }
    </script>
</body>
</html>
)rawliteral";
