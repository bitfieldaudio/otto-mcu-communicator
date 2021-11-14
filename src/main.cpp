#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <blockingconcurrentqueue.h>
#include <readerwriterqueue.h>

#include "lib/i2c.hpp"
#include "app/controller.hpp"

using namespace otto;

constexpr int packet_length = 17;

using spsc_queue = moodycamel::BlockingReaderWriterQueue<util::Packet>;
using mpmc_queue = moodycamel::BlockingConcurrentQueue<util::Packet>;

void start_new_connection(int cfd, Controller& controller) {
  std::jthread thr{[cfd, &controller](std::stop_token stop_token){
    // Create new queue
    spsc_queue from_mcu;
    controller.add_connection(&from_mcu);
    // Get other queue from controller
    mpmc_queue& to_mcu = controller.get_queue();

    std::cout << "Spawning subtask" << std::endl;

    // Spawn separate task for packets going: MCU -> client
    std::jthread child{[cfd, &from_mcu](std::stop_token child_stop_token){
      util::Packet p;
      while (!child_stop_token.stop_requested()) {

        std::cout << "Subtask: waiting to dequeue" << std::endl;
        from_mcu.wait_dequeue(p);
        // Filter out nullcmd.
        if (p.cmd != util::Command::none) write(cfd, &p, packet_length);
      }  
    }};
    // Continuously pass messages from client -> MCU 
    std::cout << "Before entering loop" << std::endl;
    while (!stop_token.stop_requested()) {
      util::Packet p;
      std::cout << "Task: waiting to read" << std::endl;
      // Zero indicates EOF (client closed connection)
      if(read(cfd, &p, packet_length) == 0) break;
      to_mcu.enqueue(p);
    }
    // Stop child task
    std::cout << "Stopping child task" << std::endl;
    child.request_stop();
    from_mcu.enqueue(util::Packet{});
    child.join();
    // Remove connection
    controller.remove_connection(&from_mcu);
  }};
  thr.detach();
}

int main(int argc, char **argv) {
  std::string path;
  std::uint16_t addr;
  // Get path and address from arguments
  po::options_description desc("MCU Communicator Service Options");
  desc.add_options()("path,p",
                     po::value<std::string>(&path)->default_value("/dev/i2c-1"),
                     "The device path (default is '/dev/i2c-1')")(
      "address,a", po::value<uint16_t>(&addr)->default_value(119),
      "I2C address in decimal (default is 119 = 0x77)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  std::chrono::duration wait_time = std::chrono::milliseconds(500);

  // Start MCUPort
  mcu_port_t port{{addr, path}};
  // Start controller
  Controller controller(std::move(port), wait_time);

  // Start socket
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd == -1) {
    std::cout << "start" << std::endl;
    return -1;
  }

  struct sockaddr_un socket_address;
  std::string socket_path = "/var/run/mcucomms";
  // Delete any file that already exists at the address. Make sure the deletion
  // succeeds. If the error is just that the file/directory doesn't exist, it's fine.
  if (remove(socket_path.c_str()) == -1 && errno != ENOENT) {
    std::cout << "socket";
    return -1;
  }
  // Zero out the address, and set family and path.
  memset(&socket_address, 0, sizeof(struct sockaddr_un));
  socket_address.sun_family = AF_UNIX;
  strncpy(socket_address.sun_path, socket_path.c_str(), sizeof(socket_address.sun_path) - 1);
  // Bind the socket to the address. Note that we're binding the server socket
  // to a well-known address so that clients know where to connect.
  if (bind(sfd, (struct sockaddr *) &socket_address, sizeof(struct sockaddr_un)) == -1) {
    std::cout << "bind" << std::endl;
    return -1;
  }
  // The listen call marks the socket as *passive*. The socket will subsequently
  // be used to accept connections from *active* sockets.
  // listen cannot be called on a connected socket (a socket on which a connect()
  // has been succesfully performed or a socket returned by a call to accept()).
  int max_waiting_connections = 5;
  if (listen(sfd, max_waiting_connections) == -1) {
    std::cout << "listen" << std::endl;
    return -1;
  }
  /* Handle client connections iteratively */
  for (;;) {          
    // Accept a connection. The connection is returned on a NEW
    // socket, 'cfd'; the listening socket ('sfd') remains open
    // and can be used to accept further connections. */
    printf("Waiting to accept a connection...\n");
    // NOTE: blocks until a connection request arrives.
    int cfd = accept(sfd, NULL, NULL);
    printf("Accepted socket fd = %d\n", cfd);
    start_new_connection(cfd, controller);
  }

}
