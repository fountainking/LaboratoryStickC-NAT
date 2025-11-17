#!/bin/bash
# Remote build script for VPS → M5StickC Plus 2 OTA deployment
# Usage: ./scripts/remote_build.sh [device_ip]

set -e

VPS_HOST="lab@172.236.230.200"
VPS_PROJECT_DIR="/home/lab/repos/LaboratoryStickC-NAT-ESPIDF"
DEVICE_IP="${1:-192.168.4.1}"  # Default to Laboratory AP IP

echo "🚀 Laboratory Remote Build & OTA Flash"
echo "========================================"
echo ""

# Step 1: Push to VPS
echo "📤 Pushing code to VPS..."
git push vps main
echo "✅ Code pushed"
echo ""

# Step 2: Build on VPS
echo "🔨 Building on VPS..."
ssh $VPS_HOST << 'ENDSSH'
cd /home/lab/repos/LaboratoryStickC-NAT-ESPIDF
echo "Pulling latest changes..."
git pull origin main
echo "Building firmware..."
pio run -e m5stick-c-plus2-espidf
echo "Build complete!"
ls -lh .pio/build/m5stick-c-plus2-espidf/firmware.bin
ENDSSH
echo "✅ Build complete"
echo ""

# Step 3: Download firmware from VPS
echo "⬇️  Downloading firmware.bin from VPS..."
scp $VPS_HOST:$VPS_PROJECT_DIR/.pio/build/m5stick-c-plus2-espidf/firmware.bin /tmp/firmware.bin
echo "✅ Downloaded to /tmp/firmware.bin"
echo ""

# Step 4: OTA upload to device
echo "📡 Uploading to device at $DEVICE_IP..."
curl -v -X POST \
  -H "Content-Type: application/octet-stream" \
  --data-binary @/tmp/firmware.bin \
  http://$DEVICE_IP/ota

echo ""
echo "✅ OTA upload complete! Device will reboot in 3 seconds."
echo ""
echo "💡 Monitor serial logs with: pio device monitor"
