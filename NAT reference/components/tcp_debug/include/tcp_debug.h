#ifndef TCP_DEBUG_H
#define TCP_DEBUG_H

#include <stdint.h>
#include <stddef.h>

#define TCP_DEBUG_PORT 8888

// Initialize TCP debug server
void tcp_debug_init(void);

// Send debug message to all connected clients
void tcp_debug_send(const char *message, size_t len);

// Printf-style debug output
void tcp_debug_printf(const char *format, ...);

#endif // TCP_DEBUG_H
