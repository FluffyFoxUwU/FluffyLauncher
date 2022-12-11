#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "bug.h"
#include "networking.h"
#include "logging/logging.h"

static int doResolve(const char* name, struct addrinfo** result) {
  struct addrinfo* lookupResult;
  int getAddrRes = getaddrinfo(name, NULL, NULL, &lookupResult);
  int res = 0;
  
  switch (getAddrRes) {
    case -EAI_AGAIN:
      res = -EAGAIN;
      goto lookup_error;
    case -EAI_MEMORY:
      res = -ENOMEM;
      goto lookup_error;
    default:
      if (res >= 0 && lookupResult != NULL)
        break;
      res = -ESRCH;
      goto lookup_error;
  }

lookup_error:
  if (result)
    *result = lookupResult;
  else if (res >= 0)
    freeaddrinfo(lookupResult);
  return res;
}

int networking_resolve(const char* name, struct ip_address* addr, int flags) {
  if ((flags & ~NETWORKING_RESOLVE_FLAG_MASK) != 0)
    return -EINVAL;
  
  struct ip_address result = {};
  int res = 0;
  struct addrinfo* lookupResultList = NULL;
  struct addrinfo* lookupResult = NULL;
  
  if ((res = doResolve(name, &lookupResultList)) < 0)
    goto resolve_error;
  lookupResult = lookupResultList;
  BUG_ON(!lookupResult);
  
  if (flags & NETWORKING_RESOLVE_PREFER_MASK) {
    int filterFor = flags & NETWORKING_RESOLVE_PREFER_IPV4 ? AF_INET : 
                    flags & NETWORKING_RESOLVE_PREFER_IPV6 ? AF_INET6 : -1;
    BUG_ON(filterFor == -1);
    
    bool foundMatch = false;
    struct addrinfo* current = lookupResultList;
    while (current) {
      if (current->ai_family == filterFor) {
        foundMatch = true;
        break;
      }
      
      current = current->ai_next;
    }
    
    lookupResult = foundMatch ? current : lookupResultList;
  }
  
  switch (lookupResult->ai_family) {
    case AF_INET:
      result.family = NETWORKING_IPV4;
      BUG_ON(lookupResult->ai_addrlen != sizeof(struct sockaddr_in));
      struct in_addr tmp1 = ((struct sockaddr_in*) lookupResult->ai_addr)->sin_addr;
      memcpy(&result.addr.ipv4, &tmp1.s_addr, sizeof(result.addr.ipv4));
      break;
    case AF_INET6:
      result.family = NETWORKING_IPV6;
      BUG_ON(lookupResult->ai_addrlen != sizeof(struct sockaddr_in6));
      struct in6_addr tmp2 = ((struct sockaddr_in6*) lookupResult->ai_addr)->sin6_addr;
      memcpy(&result.addr.ipv6, tmp2.s6_addr, sizeof(result.addr.ipv6));
      break;
  }
  
  freeaddrinfo(lookupResultList);
resolve_error:
  if (res < 0)
    pr_error("Unable to resolve %s: %d", name, res);
  
  if (addr)
    *addr = result;
  return res;
}

char* networking_tostring(struct ip_address* addr) {
  char* buffer = NULL;
  size_t len = 0;
  int family = 0;
  
  struct in6_addr asIPv6;
  struct in_addr asIPv4;
  void* addrData;
  
  switch (addr->family) {
    case NETWORKING_IPV4:
      buffer = malloc(INET_ADDRSTRLEN + 1);
      if (!buffer)
        goto fail_alloc_buffer;
      
      len = INET_ADDRSTRLEN;
      family = AF_INET;
      addrData = &asIPv4;
      memcpy(&asIPv4.s_addr, &addr->addr.ipv4, sizeof(asIPv4.s_addr));
      break;
    case NETWORKING_IPV6:
      buffer = malloc(INET6_ADDRSTRLEN + 1);
      if (!buffer)
        goto fail_alloc_buffer;
      
      len = INET6_ADDRSTRLEN;
      family = AF_INET6;
      addrData = &asIPv6;
      memcpy(asIPv6.s6_addr, &addr->addr.ipv6, sizeof(asIPv6.s6_addr));
      break;
  }
  
  if (inet_ntop(family, addrData, buffer, len) == NULL) {
    BUG_ON(errno == -ENOSPC);
    free(buffer);
    return NULL;
  }
fail_alloc_buffer:
  return buffer;
}

int networking_to_af(enum ip_family family) {
  switch (family) {
    case NETWORKING_IPV4:
      return AF_INET;
    case NETWORKING_IPV6:
      return AF_INET6;
  }
  
  BUG();
}

int networking_to_pf(enum ip_family family) {
  switch (family) {
    case NETWORKING_IPV4:
      return PF_INET;
    case NETWORKING_IPV6:
      return PF_INET6;
  }
  
  BUG();
}

int networking_to_sock_type(enum network_protocol family) {
  switch (family) {
    case NETWORKING_TCP:
      return SOCK_STREAM;
    case NETWORKING_UDP:
      return SOCK_DGRAM;
  }
  
  BUG();
}

int networking_socket(struct ip_address* addr, int sockType) {
  int res = socket(networking_to_pf(addr->family), sockType, 0);
  return res < 0 ? -errno : res;
}

int networking_connect(struct ip_address* addr, enum network_protocol protocol) {
  int socket = networking_socket(addr, networking_to_sock_type(protocol));
  if (socket < 0)
    return -EFAULT;
  
  struct sockaddr* sockAddr = NULL;
  size_t sockAddrLen = 0;
  struct sockaddr_in asIPv4 = {
    .sin_family = AF_INET
  };
  struct sockaddr_in6 asIPv6 = {
    .sin6_family = AF_INET6
  };
  switch (addr->family) {
    case NETWORKING_IPV4:
      sockAddr = (struct sockaddr*) &asIPv4;
      sockAddrLen = sizeof(asIPv4);
      memcpy(&asIPv4.sin_addr.s_addr, &addr->addr.ipv4, sizeof(asIPv4.sin_addr.s_addr));
      asIPv4.sin_port = htons(addr->port);
      break;
    case NETWORKING_IPV6:
      sockAddr = (struct sockaddr*) &asIPv6;
      sockAddrLen = sizeof(asIPv6);
      memcpy(asIPv6.sin6_addr.s6_addr, &addr->addr.ipv6, sizeof(asIPv6.sin6_addr.s6_addr));
      asIPv6.sin6_port = htons(addr->port);
      break;
  }
  
  int res = connect(socket, sockAddr, sockAddrLen);
  if (res < 0) {
    res = -errno;
    switch (errno) {
      case ENETUNREACH:
      case ETIMEDOUT:
      case ENETDOWN:
      case ECONNREFUSED:
      case EHOSTUNREACH:
        break;
      default:
        res = -EFAULT;
        break;
    }
  }
  return res < 0 ? res : socket;
}

