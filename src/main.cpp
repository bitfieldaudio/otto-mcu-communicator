#include <boost/program_options.hpp>
#include <ostream>
namespace po = boost::program_options;

#include <csignal>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <optional>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <thread>

#include "app/controller.hpp"
#include "lib/domain_socket.hpp"
#include "lib/i2c.hpp"
#include "lib/socket_pool.hpp"

using namespace otto;

void sigTermHandler(int signum) {
  std::cout << "Interrupt signal (" << signum << ") received.\n";
  // Clean up
  exit(signum);
}

void handle_buffer(SocketPool::buffer_type &buf) {
  for (auto &p : buf) {
    switch (p.cmd) {
    case util::Command{239}: {
      p.cmd = util::Command::none; // HACK
      break;
    }
    case util::Command::shutdown: {
      std::system("poweroff -d 1");
      break;
    }
    default:
      break;
    }
  }
}

int main(int argc, char **argv) {
  // Parse command-line arguments
  std::string i2c_dev_path;
  std::uint16_t addr;
  std::string socket_path;
  int wait_time_ms;
  std::optional<std::string> log = std::nullopt;
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
      "I2C polling wait time in milliseconds (default is 1)")(
      "log,l",
      po::value<std::string>(),
      "Log communication to file");

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

  // Register signal handler
  ::signal(SIGTERM, sigTermHandler);

  // Handle logging
  if (vm.count("log")) {
    log.emplace(vm["log"].as<std::string>());
  }
  if (log) {
    std::ofstream log_stream(log.value());
    std::streambuf *coutbuf = std::cout.rdbuf(); //In case we want to reset to stdout?
    std::cout.rdbuf(log_stream.rdbuf()); //redirect std::cout to log
  }

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
    if (new_sd)
      sockets.add_connection(new_sd.value());

    // Handle buffer
    // Pass data: MCU -> Clients
    auto &buf = port.read_buffer(log.has_value());
    handle_buffer(buf);
    sockets.distribute_buffer(buf);
    // Pass data Clients -> MCU
    port.write_buffer(sockets.get_all_packets(), log.has_value());

    // Sleep
    std::this_thread::sleep_for(wait_time);
  }

  // TODO: Clean up any open connections
}
