#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Start DNS hijack server
 * All DNS queries will be answered with 192.168.4.1
 * This triggers captive portal popup on phones
 */
void dns_server_start(void);

/**
 * Enable or disable captive portal hijacking
 * When disabled, all queries are forwarded to upstream DNS
 * Call this after user successfully configures WiFi
 */
void dns_set_captive_mode(bool enable);

/**
 * Approve a client for internet access
 * Approved clients get DNS forwarded to 8.8.8.8
 * Unapproved clients get captive portal redirects
 */
void dns_approve_client(uint32_t client_ip);

/**
 * Check if a client is approved for internet access
 */
bool dns_is_client_approved(uint32_t client_ip);

#endif // DNS_SERVER_H
