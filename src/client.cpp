#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <magic_enum.hpp>
#include "lib/i2c.hpp"

int main(int argc, char *argv[]) {

  std::string socket_path = "/var/run/mcucomms";
  struct sockaddr_un addr;

  // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  printf("Client socket fd = %d\n", sfd);
  if (sfd == -1) {
    std::cout << "start" << std::endl;
  }

  //
  // Construct server address, and make the connection.
  //
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  // Connects the active socket referred to be sfd to the listening socket
  // whose address is specified by addr.
  if (connect(sfd, (struct sockaddr *) &addr,
              sizeof(struct sockaddr_un)) == -1) {
    std::cout << "connect" << std::endl;
  }

  //
  // Continuously read from socket
  //
  otto::util::Packet p;
  constexpr int packet_length = 17;
  size_t numRead;
  while ((numRead = read(sfd, &p, packet_length)) > 0) {
    // Then, write those bytes to stdout
    std::cout << "Byte from socket: " << magic_enum::enum_integer(p.cmd) << " : ";
    for (auto& elem : p.data) {
      std::cout << elem << " ";
    }
    std::cout << std::endl;
  }

  // Closes our socket; server sees EOF.
  exit(EXIT_SUCCESS);
}