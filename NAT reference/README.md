# Laboratory StickC Plus 2

**Standalone captive portal broadcaster for conventions**

![Flash Usage](https://img.shields.io/badge/Flash-99.2%25-red)
![RAM Usage](https://img.shields.io/badge/RAM-22.8%25-green)
![Version](https://img.shields.io/badge/version-1.0.0-yellow)

## Features

- 🌐 **Laboratory Captive Portal** - Broadcast custom portals with visitor tracking
- 📡 **WiFi Setup Portal** - Network scanning and connection via captive portal
- 📤 **Transfer Server** - Upload custom portal HTML via web browser
- 📁 **File Browser** - Manage portal HTML and media files
- ⚙️ **Settings** - WiFi, Sound, Dim, OTA Updates
- 🔄 **OTA Updates** - Update firmware from GitHub releases
- 💾 **WiFi Memory** - Auto-connect to saved networks on boot
- 🔋 **Battery Aware** - Automatic dimming when battery < 20%

## Hardware

- **Device:** M5StickC Plus 2
- **Display:** 240x135 LCD (landscape)
- **Buttons:**
  - **BtnA** (front) - Select/Enter
  - **BtnB** (side) - Back/Cancel (hold 2.5s to power off)
  - **BtnC** (GPIO35) - Navigate

## Navigation

### Main Menu
- **BtnC** - Cycle through options
- **BtnA** - Select option

### Files
- **BtnC** - Navigate items
- **BtnA** - Toggle folder expansion / View README
- **BtnB** - Back

### Settings
- **BtnC** - Navigate settings
- **BtnA** - Toggle setting / Launch action
- **BtnB** - Back

## Memory Limits

- **HTML:** 100KB max
- **Media:** 2MB max
- **Flash:** 99.2% used (~10KB free)

## OTA Updates

Device checks GitHub releases for updates:
- Connect to WiFi via Settings
- Navigate to Settings → Update
- Device checks for new releases
- Press **BtnA** to install update

### Creating a Release

1. Build firmware: `pio run`
2. Copy `.pio/build/m5stick-c-plus2/firmware.bin`
3. Create GitHub release with tag (e.g., `v1.0.1`)
4. Upload `firmware.bin` to release
5. Devices will detect and offer to install

**Mandatory Updates:** Add `[MANDATORY]` or `[CRITICAL]` to release name

## Build

```bash
pio run
pio run --target upload
```

## License

Built with ❤️ for Laboratory
