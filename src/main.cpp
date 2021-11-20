#include <boost/program_options.hpp>
#include <ostream>
namespace po = boost::program_options;

#include <errno.h>
#include <iostream>
#include <sys/socket.h>

#include <blockingconcurrentqueue.h>
#include <readerwriterqueue.h>

#include "app/controller.hpp"
#include "lib/domain_socket.hpp"
#include "lib/i2c.hpp"

/*
TODO: Controller access should be wrapped in a mutex.
TODO: Use ppoll to be able to catch SIGTERM on the main thread (the one calling access() )
*/

using namespace otto;

constexpr int packet_length = 17;

using spsc_queue = moodycamel::BlockingReaderWriterQueue<util::Packet>;
using mpmc_queue = moodycamel::BlockingConcurrentQueue<util::Packet>;

std::jthread start_new_connection(int cfd, Controller &controller) {
  std::jthread thr{[cfd, &controller](std::stop_token stop_token) {
    // Create new queue
    spsc_queue from_mcu;
    controller.add_connection(&from_mcu);
    // Get other queue from controller
    mpmc_queue &to_mcu = controller.get_queue();

    std::cout << "Spawning subtask" << std::endl;

    // Spawn separate task for packets going: MCU -> client
    std::jthread child{[cfd, &from_mcu](std::stop_token child_stop_token) {
      util::Packet p;
      while (!child_stop_token.stop_requested()) {
        from_mcu.wait_dequeue(p);
        write(cfd, &p, packet_length);
      }
    }};
    // Continuously pass messages from client -> MCU
    while (!stop_token.stop_requested()) {
      util::Packet p;
      // Zero indicates EOF (client closed connection)
      if (read(cfd, &p, packet_length) == 0)
        break;
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
  return thr;
}

int main(int argc, char **argv) {

  // Parse command-line arguments
  std::string i2c_dev_path;
  std::uint16_t addr;
  std::string socket_path;
  int wait_time_ms;
  po::options_description desc("MCU Communicator Service Options");
  desc.add_options()(
      "help,h", "Print this help message")(
      "device,d",
      po::value<std::string>(&i2c_dev_path)->default_value("/dev/i2c-1"),
      "The I2Cdevice (default is '/dev/i2c-1')")(
      "address,a", po::value<uint16_t>(&addr)->default_value(119),
      "I2C address in decimal (default is 119 = 0x77)")(
      "socket,s",
      po::value<std::string>(&socket_path)->default_value("/run/otto-mcu-communicator.sock"),
      "Socket path (default is /run/mcucomms)")(
      "wait_time,w", po::value<int>(&wait_time_ms)->default_value(1),
      "I2C polling wait time in milliseconds (default is 1)");

  // Parse command-line arguments
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
          options(desc).run(), vm);
  po::notify(vm);
  // Check for 'help' argument
  if (vm.count ("help")) {
        std::cerr << desc << "\n";
        return 1;
    }

  std::chrono::duration wait_time = std::chrono::milliseconds(wait_time_ms);

  // Start MCUPort
  mcu_port_t port{{addr, i2c_dev_path}};
  // Start controller
  Controller controller(std::move(port), wait_time);

  // Start socket
  int sfd = create_socket(socket_path);
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
  /* Handle client connections iteratively */
  std::vector<std::jthread> thread_handles;
  for (;;) {
    // Accept a connection. The connection is returned on a NEW
    // socket, 'cfd'; the listening socket ('sfd') remains open
    // and can be used to accept further connections. */
    printf("Waiting to accept a connection...\n");
    // NOTE: blocks until a connection request arrives.
    int cfd = accept(sfd, NULL, NULL);
    printf("Accepted socket fd = %d\n", cfd);
    // Garbage collection
    std::erase_if(thread_handles, [](std::jthread& thr){return !thr.joinable(); });
    // Make new task
    thread_handles.push_back(start_new_connection(cfd, controller));
  }
}
