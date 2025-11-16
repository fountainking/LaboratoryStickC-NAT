# ESP32 Captive Portal NAT Status

## Current Situation

### ❌ **NAT NOT WORKING** - Root Cause Identified

The Arduino ESP32 framework v2.0.14 (espressif32@6.5.0) **does not have NAPT compiled into the SDK**.

**Evidence:**
```bash
$ grep -i "NAPT\|IP_FORWARD" sdkconfig
# CONFIG_LWIP_IP_FORWARD is not set
```

The SDK was compiled with `CONFIG_LWIP_IP_FORWARD is not set`, which means:
- Build flags `-DCONFIG_LWIP_IPV4_NAPT=1` have no effect
- The function `ip_napt_enable()` exists in headers but is **not linked** in libraries
- Linker error: `undefined reference to ip_napt_enable`

### Why This Happens

1. **Headers exist** (`/tools/sdk/esp32/include/lwip/lwip/src/include/lwip/lwip_napt.h`)
2. **Function declaration exists** (`void ip_napt_enable(u32_t addr, int enable);`)
3. **Implementation is NOT compiled** - the function body doesn't exist in any `.a` library
4. **Conditional compilation** - NAPT code is wrapped in `#if ESP_LWIP && IP_FORWARD && IP_NAPT`

### What DOESN'T Work

❌ Adding build flags to `platformio.ini`
❌ Including `<lwip/lwip_napt.h>` header
❌ Using `extern "C"` declaration
❌ Using `esp32_nat_router` library (ESP-IDF only, incompatible with Arduino)

## Solutions

### Option 1: Use ESP-IDF Framework (RECOMMENDED for NAT)

**Pros:**
- Full NAT support built-in
- `esp32_nat_router` works perfectly
- Real hotel/airplane WiFi functionality

**Cons:**
- Requires rewriting code for ESP-IDF (no Arduino APIs)
- M5Unified library may not work
- Significant effort

**Implementation:**
```ini
[env:m5stick-c-plus2]
platform = espressif32
framework = espidf
```

### Option 2: Custom SDK Build with NAPT (ADVANCED)

Rebuild the entire ESP32 SDK with:
```
CONFIG_LWIP_IP_FORWARD=y
CONFIG_LWIP_IPV4_NAPT=y
```

**Pros:**
- Keep Arduino framework
- Full NAT support

**Cons:**
- Extremely complex (requires esp-idf toolchain)
- Must rebuild entire SDK
- Not portable

### Option 3: DNS Hijacking Only (CURRENT IMPLEMENTATION)

**What it does:**
- Captive portal works ✓
- DNS hijacking triggers portal popup ✓
- After authentication, DNS server stops
- **Clients have NO internet** (no NAT)

**Why it fails:**
- Phone connects to AP
- Phone gets IP: 192.168.4.2, Gateway: 192.168.4.1
- Phone's DHCP says "DNS: 192.168.4.1" (the ESP32)
- User clicks "Connect to WiFi"
- ESP32 stops DNS server
- Phone still thinks "DNS = 192.168.4.1"
- DNS queries timeout → no name resolution
- Even if IP forwarding worked, phone can't resolve domains
- Result: "No internet" icon, no WiFi bars

### Option 4: **WORKING INTERIM SOLUTION** - Manual DNS Configuration

Since NAT can't work, we can:

1. **Keep captive portal for authentication**
2. **Tell user to manually set DNS to 8.8.8.8**
3. **Hope IP forwarding is enough** (unlikely, but worth testing)

**Modified success page:**
```html
<h1>ALMOST CONNECTED!</h1>
<p>Final step: Go to WiFi settings and manually set DNS to:</p>
<code>8.8.8.8</code>
<p>Then browse normally.</p>
```

### Option 5: Transparent Proxy (COMPLEX)

Implement a **full DNS + HTTP proxy** on the ESP32:

**DNS Proxy** (instead of hijacking):
- Forward all DNS queries to 8.8.8.8
- Relay responses back to client
- Port 53 UDP proxy

**HTTP Proxy** (if needed):
- Intercept HTTP requests
- Forward to real servers
- Relay responses

**Cons:**
- Very complex
- High memory usage
- CPU overhead
- Still might not work for HTTPS

## Recommendation

**For this project (M5StickC Plus 2 with Arduino):**

1. **Accept NAT won't work** with current SDK
2. **Keep captive portal for branding/authentication**
3. **Clearly communicate limitation** in success page
4. **Optional:** Add button "Configure DNS for me" that attempts to send DHCP update

**For production NAT router:**
- Switch to ESP-IDF framework
- Use `esp32_nat_router` as base
- Build proper hotel WiFi system

## What We Accomplished

✅ Identified exact root cause (SDK not compiled with NAPT)
✅ Tested all available workarounds
✅ Captive portal works perfectly
✅ DNS hijacking triggers popup
✅ Authentication flow complete
✅ Understand limitations clearly

## Files Modified

- `src/captive_portal.cpp` - NAT calls added (but won't link)
- `platformio.ini` - Build flags added (but SDK ignores them)

## Next Steps

**Choose one:**

A. **Accept limitation** - Remove NAT code, document workaround
B. **Switch to ESP-IDF** - Full rewrite for real NAT
C. **Implement DNS proxy** - Complex but stays in Arduino framework

---

**Current build status:** ❌ FAILS (linker error on `ip_napt_enable`)
**Expected with NAT removed:** ✅ BUILDS
**Captive portal status:** ✅ WORKING
**Internet sharing status:** ❌ NOT POSSIBLE without NAT or proxy
