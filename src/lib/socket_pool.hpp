#include <vector>

#include "lib/i2c.hpp"

namespace otto {

  struct SocketPool {
    using file_descriptor = int;
    using buffer_type = std::vector<util::Packet>;

    void add_connection(file_descriptor);
    
    // Get all data from connections
    // Data is ensured to be contiguous for each connection
    buffer_type& get_all_packets();

    void distribute_buffer(const buffer_type&);

    const bool empty() noexcept {return handles.empty();}

  private:
    void purge_connections();
    bool add_packets_from_conn(file_descriptor, buffer_type&);
    std::vector<file_descriptor> handles;
    buffer_type buffer;
  };

}