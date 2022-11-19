#ifndef _headers_1667649555_FluffyLauncher_networking
#define _headers_1667649555_FluffyLauncher_networking

#include <stdint.h>

enum ip_family {
  NETWORKING_IPV4,
  NETWORKING_IPV6
};

enum network_protocol {
  NETWORKING_TCP,
  NETWORKING_UDP
};

typedef struct {
  uint8_t data[4];
} networking_ipv4;

typedef struct {
  uint8_t data[16];
} networking_ipv6;

struct ip_address {
  enum ip_family family;
  uint16_t port;
  
  // Not in network byte order
  union {
    networking_ipv4 ipv4;
    networking_ipv6 ipv6;
  } addr;
};

#define NETWORKING_RESOLVE_PREFER_IPV4 0b0001
#define NETWORKING_RESOLVE_PREFER_IPV6 0b0010
#define NETWORKING_RESOLVE_PREFER_MASK (NETWORKING_RESOLVE_PREFER_IPV4 | NETWORKING_RESOLVE_PREFER_IPV6)

#define NETWORKING_RESOLVE_FLAG_MASK   0b0011

// Return 0 on success
// Errors:
// -ESRCH: Can't be resolved
// -EINVAL: Invalid flags
// -EAGAIN: Try again later
[[nodiscard]]
int networking_resolve(const char* name, struct ip_address* addr, int flags);

// Convert address to malloc'ed textual representation or NULL on error

[[nodiscard]] 
char* networking_tostring(struct ip_address* addr);

int networking_to_af(enum ip_family family);
int networking_to_pf(enum ip_family family);
int networking_to_sock_type(enum network_protocol family);

// Wrapper around `socket`
[[nodiscard]]
int networking_socket(struct ip_address* addr, int sockType);

// Return socket fd on success
// Errors:
// -ENETUNREACH: Network unreachable
// -ETIMEDOUT: Time out
// -ENETDOWN: Network unavailable
// -ECONNREFUSED: Connnection refused
// -EHOSTUNREACH: Host is down
[[nodiscard]]
int networking_connect(struct ip_address* addr, enum network_protocol protocol);

#endif

