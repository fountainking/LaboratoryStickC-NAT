#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdbool.h>

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

#endif // DNS_SERVER_H
