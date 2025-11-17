# 📱 Mobile Development Workflow

Code from your phone → VPS → OTA to M5StickC Plus 2 while on the go!

## Setup Complete ✅

### VPS Git Remote
```bash
git remote add vps lab@172.236.230.200:/home/lab/repos/LaboratoryStickC-NAT-ESPIDF.git
```

### OTA Endpoints
- **Upload page**: http://192.168.4.1/update
- **OTA upload**: POST http://192.168.4.1/ota (binary firmware)
- **Debug logs**: http://192.168.4.1/debug/logs
- **Recent logs**: http://192.168.4.1/debug/recent?lines=20

## Workflow

### Option 1: Automated Build + Flash
```bash
# From laptop (pushes to VPS, builds, downloads, OTA flashes)
./scripts/remote_build.sh

# Or specify device IP if not on Laboratory AP
./scripts/remote_build.sh 192.168.1.100
```

### Option 2: Manual Phone Workflow
1. **Code on phone** (via Termux, SSH, or mobile IDE)
2. **Push to VPS**:
   ```bash
   git push vps main
   ```
3. **SSH to VPS and build**:
   ```bash
   ssh lab@172.236.230.200
   cd /home/lab/repos/LaboratoryStickC-NAT-ESPIDF
   git pull
   pio run -e m5stick-c-plus2-espidf
   ```
4. **Get firmware URL from VPS** (set up HTTPS server or use SCP)
5. **Connect phone to "Laboratory" WiFi AP**
6. **Open browser → http://192.168.4.1/update**
7. **Upload firmware.bin** downloaded from VPS
8. **Device reboots** with new code!

### Option 3: Direct GitHub Releases (Future)
Configure `components/ota_manager/include/ota_manager.h`:
```c
#define GITHUB_API_URL "https://api.github.com/repos/fountainking/LaboratoryStickC-NAT/releases/latest"
```

Create release with `firmware.bin` asset, device auto-pulls via GitHub API.

## VPS Setup Required

### 1. Initialize bare git repo on VPS
```bash
ssh lab@172.236.230.200
mkdir -p /home/lab/repos
cd /home/lab/repos
git init --bare LaboratoryStickC-NAT-ESPIDF.git
```

### 2. Clone working copy for builds
```bash
cd /home/lab
git clone /home/lab/repos/LaboratoryStickC-NAT-ESPIDF.git build-workspace
cd build-workspace
```

### 3. Install PlatformIO on VPS
```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py
export PATH=$PATH:~/.platformio/penv/bin
```

### 4. Set up HTTPS firmware server (optional)
```bash
# Simple Python HTTPS server for firmware downloads
cd /home/lab/build-workspace/.pio/build/m5stick-c-plus2-espidf
python3 -m http.server 8443 --bind 0.0.0.0
```

## Partition Layout

**partitions_ota.csv**:
- `factory` (1MB) - Initial boot partition
- `ota_0` (1MB) - First OTA slot
- `ota_1` (1MB) - Second OTA slot (rollback)

Total: 3MB for firmware (M5StickC Plus 2 has 16MB flash)

## Color Theme System

Random theme selected on boot. Available themes:
- RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, PURPLE

Each theme has dark→light gradient. Theme name displayed on boot and in debug screen:
```
LAB NAT [CYAN]
```

## Testing OTA

1. **Build initial firmware**:
   ```bash
   pio run -e m5stick-c-plus2-espidf -t upload
   ```

2. **Make code change** (e.g., modify theme colors)

3. **Run remote build**:
   ```bash
   ./scripts/remote_build.sh
   ```

4. **Watch device reboot** with new theme color!

## Troubleshooting

### Device not appearing at 192.168.4.1
- Check if connected to "Laboratory" AP
- Verify AP is broadcasting (should be open network)
- Check device logs: `pio device monitor`

### OTA upload fails
- Firmware too large? Check partition size (max 1MB)
- Verify OTA partition table: `esptool.py --port /dev/cu.usbserial-* read_flash 0x8000 0xC00 partition.bin`
- Check device logs for OTA errors

### VPS build fails
- Ensure PlatformIO installed: `pio --version`
- Install ESP-IDF platform: `pio platform install espressif32@6.5.0`
- Check Python version: `python3 --version` (needs 3.7+)

### Colors still wrong
- Verify byte-swap in `m5_display.c:349-361`
- Check theme gradient calculation in `color_theme.c`
- Monitor boot logs for theme selection

## Next Steps

- [ ] Set up GitHub Actions for automatic releases
- [ ] Add mDNS for `laboratory.local` access
- [ ] Implement firmware rollback on boot failure
- [ ] Add progress bar to OTA web UI
- [ ] Create Termux shortcuts for phone workflow
