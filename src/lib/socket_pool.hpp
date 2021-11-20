#include <vector>

#include "lib/i2c.hpp"

namespace otto {

  struct SocketPool {
    using file_descriptor = int;
    using buffer_type = std::vector<util::Packet>;

    void add_connection(file_descriptor);
    void remove_connection(file_descriptor);
    void purge_connections();

    // Get all data from connections
    // Data is ensured to be contiguous for each connection
    void add_packets_from_conn(file_descriptor, buffer_type&);
    buffer_type& get_all_packets();

    void distribute_packet(util::Packet&);
    void distribute_buffer(buffer_type&);

    const bool empty() noexcept {return handles.empty();}

  private:
    std::vector<file_descriptor> handles;
    std::vector<file_descriptor> handles_to_remove;
    buffer_type buffer;
  };

}