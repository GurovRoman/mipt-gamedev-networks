#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>

#include "socket_tools.h"

// Adaptation of linux man page: https://linux.die.net/man/3/getaddrinfo
static int get_dgram_socket(addrinfo *addr, bool should_bind, addrinfo *res_addr)
{
  for (addrinfo *ptr = addr; ptr != nullptr; ptr = ptr->ai_next)
  {
    if (ptr->ai_family != AF_INET || ptr->ai_socktype != SOCK_DGRAM || ptr->ai_protocol != IPPROTO_UDP)
      continue;
    int sfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sfd == -1)
      continue;

    fcntl(sfd, F_SETFL, O_NONBLOCK);

    int trueVal = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &trueVal, sizeof(int));

    if (res_addr)
      *res_addr = *ptr;
    if (!should_bind)
      return sfd;

    if (bind(sfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
      return sfd;

    close(sfd);
  }
  return -1;
}

int create_dgram_socket(const char *address, const char *port, addrinfo *res_addr)
{
  addrinfo hints {};

  bool isListener = !address;

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  if (isListener)
    hints.ai_flags = AI_PASSIVE;

  addrinfo *result = nullptr;
  if (getaddrinfo(address, port, &hints, &result) != 0)
    return 1;

  int sfd = get_dgram_socket(result, isListener, res_addr);

  freeaddrinfo(result);
  return sfd;
}

