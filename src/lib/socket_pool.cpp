#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

#include <fmt/format.h>

#include "lib/socket_pool.hpp"

constexpr int packet_length = 17;

namespace otto {

void SocketPool::add_connection(file_descriptor fd) { handles.push_back(fd); }

void SocketPool::purge_connections() {
  std::erase_if(handles, [](auto& fd){return fd < 0;});
}

void SocketPool::distribute_buffer(const buffer_type &packets) {
  for (auto &fd : handles) {
    for (auto &p : packets) {
      if (::send(fd, &p, packet_length, 0) == -1) fd = -1;
    }  
  }
  purge_connections();
}

// Returns true if file descriptor should be removed from the pool
bool SocketPool::add_packets_from_conn(file_descriptor fd, buffer_type &buf) {
  util::Packet p;
  while (true) {
    /*
    Receive data on this connection until the
    recv fails with EWOULDBLOCK.
    */
    int rc = ::recv(fd, &p, packet_length, MSG_DONTWAIT);
    /*
    3 possibilities: 
    - successful read
    - error+EWOULDBLOCK which means no new data
      - any other error indicate a problem
    - socket peer has closed down (rc = 0)
    */
    if (rc < 0) {
      if (errno != EWOULDBLOCK) return true;
      else return false;
    }
    if (rc == 0) return true;
    buf.push_back(p);
    return false;
  }
}

SocketPool::buffer_type &SocketPool::get_all_packets() {
  buffer.clear();
  for (auto &conn : handles) {
    if(add_packets_from_conn(conn, buffer)) conn = -1;
  }
  purge_connections();
  return buffer;
}
} // namespace otto