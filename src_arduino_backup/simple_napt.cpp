/**
 * Simple NAPT (Network Address Port Translation) Implementation
 *
 * This is a minimal NAT implementation that will translate packets
 * from the AP network (192.168.4.x) to the upstream WiFi network.
 */

#include <Arduino.h>
#include <WiFi.h>

// NAPT translation table entry
struct napt_entry {
    uint32_t src_addr;      // Original source IP
    uint16_t src_port;      // Original source port
    uint16_t mapped_port;   // Mapped external port
    uint8_t  proto;         // Protocol (TCP=6, UDP=17)
    uint32_t last_active;   // Timestamp for cleanup
    bool     in_use;
};

#define NAPT_TABLE_SIZE 256
#define NAPT_TIMEOUT_MS 60000

static napt_entry napt_table[NAPT_TABLE_SIZE];
static uint16_t next_port = 50000;

// Initialize NAPT
void napt_init() {
    memset(napt_table, 0, sizeof(napt_table));
    Serial.println("[NAPT] Initialized translation table");
}

// Find or create NAT mapping
int napt_find_or_create(uint32_t src_ip, uint16_t src_port, uint8_t proto) {
    uint32_t now = millis();
    int free_slot = -1;

    // First, look for existing mapping
    for (int i = 0; i < NAPT_TABLE_SIZE; i++) {
        if (napt_table[i].in_use) {
            // Cleanup expired entries
            if (now - napt_table[i].last_active > NAPT_TIMEOUT_MS) {
                napt_table[i].in_use = false;
                continue;
            }

            // Match found
            if (napt_table[i].src_addr == src_ip &&
                napt_table[i].src_port == src_port &&
                napt_table[i].proto == proto) {
                napt_table[i].last_active = now;
                return i;
            }
        } else if (free_slot == -1) {
            free_slot = i;
        }
    }

    // Create new mapping
    if (free_slot != -1) {
        napt_table[free_slot].src_addr = src_ip;
        napt_table[free_slot].src_port = src_port;
        napt_table[free_slot].mapped_port = next_port++;
        napt_table[free_slot].proto = proto;
        napt_table[free_slot].last_active = now;
        napt_table[free_slot].in_use = true;

        if (next_port > 65000) next_port = 50000; // Wrap around

        return free_slot;
    }

    return -1; // Table full
}

// Reverse lookup: find original destination from mapped port
int napt_reverse_lookup(uint16_t mapped_port, uint8_t proto) {
    for (int i = 0; i < NAPT_TABLE_SIZE; i++) {
        if (napt_table[i].in_use &&
            napt_table[i].mapped_port == mapped_port &&
            napt_table[i].proto == proto) {
            napt_table[i].last_active = millis();
            return i;
        }
    }
    return -1;
}

// Get stats
void napt_get_stats() {
    int active = 0;
    for (int i = 0; i < NAPT_TABLE_SIZE; i++) {
        if (napt_table[i].in_use) active++;
    }
    Serial.printf("[NAPT] Active mappings: %d/%d\n", active, NAPT_TABLE_SIZE);
}
