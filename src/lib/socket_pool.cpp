#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

#include <fmt/format.h>

#include "lib/socket_pool.hpp"

constexpr int packet_length = 17;

namespace otto {

void SocketPool::add_connection(file_descriptor fd) { handles.push_back(fd); }

void SocketPool::remove_connection(file_descriptor fd) {
  ::close(fd);
  auto it = std::ranges::find(handles, fd);
  if (it != handles.end())
    handles.erase(it);
}

void SocketPool::purge_connections() {
  for (auto fd : handles_to_remove)
    remove_connection(fd);
  handles_to_remove.clear();
}

void SocketPool::distribute_buffer(buffer_type &packets) {
  for (auto &p : packets) {
    distribute_packet(p);
  }
}

void SocketPool::distribute_packet(util::Packet &p) {
  for (auto &fd : handles) {
    int ret = ::send(fd, &p, packet_length, 0);
    if (ret == -1)
      handles_to_remove.push_back(fd);
  }
  purge_connections();
}

void SocketPool::add_packets_from_conn(file_descriptor fd, buffer_type &buf) {
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
    - socket peer has closed down
    */
    if (rc < 0) {
      if (errno != EWOULDBLOCK) handles_to_remove.push_back(fd);
      break;
    }
    if (rc == 0) {
      handles_to_remove.push_back(fd);
      break;
    }
    buf.push_back(p);
  }
}

SocketPool::buffer_type &SocketPool::get_all_packets() {
  buffer.clear();
  for (auto &conn : handles) {
    add_packets_from_conn(conn, buffer);
  }
  purge_connections();
  return buffer;
}
} // namespace otto