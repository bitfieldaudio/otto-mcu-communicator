#include <boost/program_options.hpp>
#include <ostream>
namespace po = boost::program_options;

#include <errno.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <optional>
#include <thread>

#include "app/controller.hpp"
#include "lib/domain_socket.hpp"
#include "lib/socket_pool.hpp"
#include "lib/i2c.hpp"

using namespace otto;

int main(int argc, char **argv) {
  // Parse command-line arguments
  std::string i2c_dev_path;
  std::uint16_t addr;
  std::string socket_path;
  int wait_time_ms;
  po::options_description desc("MCU Communicator Service Options");
  desc.add_options()("help,h", "Print this help message")(
      "device,d",
      po::value<std::string>(&i2c_dev_path)->default_value("/dev/i2c-1"),
      "The I2Cdevice (default is '/dev/i2c-1')")(
      "address,a", po::value<uint16_t>(&addr)->default_value(119),
      "I2C address in decimal (default is 119 = 0x77)")(
      "socket,s",
      po::value<std::string>(&socket_path)
          ->default_value("/run/otto-mcu-communicator.sock"),
      "Socket path (default is /run/mcucomms)")(
      "wait_time,w", po::value<int>(&wait_time_ms)->default_value(1),
      "I2C polling wait time in milliseconds (default is 1)");

  // Parse command-line arguments
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);
  // Check for 'help' argument
  if (vm.count("help")) {
    std::cerr << desc << "\n";
    return 1;
  }

  std::chrono::duration wait_time = std::chrono::milliseconds(wait_time_ms);

  // Start MCUPort
  mcu_port_t port{{addr, i2c_dev_path}};
  
  // Start nonblocking socket
  int sfd = create_socket(socket_path, true);
  if (sfd == -1) {
    std::cout << "Socket could not start" << std::endl;
    return -1;
  }
  // Activate socket
  int ret = listen_to_socket(sfd);
  if (ret) {
    std::cout << "Socket could not be listened to" << std::endl;
    return -1;
  }

  /*************************************************************/
  /* Loop waiting for incoming connects, data from the MCU,    */
  /* or for incoming data on any of the connected sockets.     */
  /*************************************************************/
  SocketPool sockets;
  while (true) {
    // Accept new connections
    std::optional<int> new_sd = accept_new_connection(sfd);
    if (new_sd) sockets.add_connection(new_sd.value());

    if (!sockets.empty()) {
      // Pass data: MCU -> Clients
      sockets.distribute_buffer(port.read_buffer());
      // Pass data Clients -> MCU
      port.write_buffer(sockets.get_all_packets());
    }
    
    // Sleep
    std::this_thread::sleep_for(wait_time);
  }

  // TODO: Clean up any open connections

}
