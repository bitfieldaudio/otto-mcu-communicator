#include <iostream>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <optional>
#include <string>

#include "lib/domain_socket.hpp"

namespace otto {

int create_socket(std::string socket_path, bool nonblocking) {
  // Start socket
  int sfd =
      socket(AF_UNIX, nonblocking ? SOCK_STREAM | SOCK_NONBLOCK : SOCK_STREAM, 0);
  if (sfd == -1) {
    std::cout << "socket() failed" << std::endl;
    return -1;
  }

  // Delete any file that already exists at the address. Make sure the deletion
  // succeeds. If the error is just that the file/directory doesn't exist, it's
  // fine.
  if (remove(socket_path.c_str()) == -1 && errno != ENOENT) {
    std::cout << "remove() on socket path failed";
    return -1;
  }
  // Zero out the address, and set family and path.
  struct sockaddr_un socket_address;
  memset(&socket_address, 0, sizeof(struct sockaddr_un));
  socket_address.sun_family = AF_UNIX;
  strncpy(socket_address.sun_path, socket_path.c_str(),
          sizeof(socket_address.sun_path) - 1);
  // Bind the socket to the address. Note that we're binding the server socket
  // to a well-known address so that clients know where to connect.
  if (bind(sfd, (struct sockaddr *)&socket_address,
           sizeof(struct sockaddr_un)) == -1) {
    std::cout << "bind() failed" << std::endl;
    return -1;
  }
  return sfd;
}

int listen_to_socket(int sfd) {
  // The listen call marks the socket as *passive*. The socket will subsequently
  // be used to accept connections from *active* sockets.
  // listen cannot be called on a connected socket (a socket on which a
  // connect() has been succesfully performed or a socket returned by a call to
  // accept()).
  int max_waiting_connections = 5;
  if (listen(sfd, max_waiting_connections) == -1) {
    std::cout << "listen" << std::endl;
    return -1;
  }
  return 0;
}

std::optional<int> accept_new_connection(int sfd) {
  int new_sd = accept(sfd, NULL, NULL);
  if (new_sd < 0) {
    if (errno != EWOULDBLOCK)
      printf("accept() failed");
    return std::nullopt;
  } else {
    // New connection
    std::cout << "New connection established" << std::endl;
    return std::optional<int>(new_sd);
  }
}
} // namespace otto
