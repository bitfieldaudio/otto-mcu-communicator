#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <errno.h>
#include <iostream>
#include <sys/socket.h>

#include <blockingconcurrentqueue.h>
#include <readerwriterqueue.h>

#include "app/controller.hpp"
#include "lib/i2c.hpp"
#include "lib/domain_socket.hpp"

using namespace otto;

constexpr int packet_length = 17;

using spsc_queue = moodycamel::BlockingReaderWriterQueue<util::Packet>;
using mpmc_queue = moodycamel::BlockingConcurrentQueue<util::Packet>;

void start_new_connection(int cfd, Controller &controller) {
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

        std::cout << "Subtask: waiting to dequeue" << std::endl;
        from_mcu.wait_dequeue(p);
        // Filter out nullcmd.
        if (p.cmd != util::Command::none)
          write(cfd, &p, packet_length);
      }
    }};
    // Continuously pass messages from client -> MCU
    std::cout << "Before entering loop" << std::endl;
    while (!stop_token.stop_requested()) {
      util::Packet p;
      std::cout << "Task: waiting to read" << std::endl;
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
  thr.detach();
}

int main(int argc, char **argv) {

  // Parse command-line arguments
  std::string i2c_dev_path;
  std::uint16_t addr;
  std::string socket_path;
  po::options_description desc("MCU Communicator Service Options");
  desc.add_options()(
      "device,d",
      po::value<std::string>(&i2c_dev_path)->default_value("/dev/i2c-1"),
      "The I2Cdevice (default is '/dev/i2c-1')")(
      "address,a", po::value<uint16_t>(&addr)->default_value(119),
      "I2C address in decimal (default is 119 = 0x77)")(
      "socket,s",
      po::value<std::string>(&socket_path)->default_value("/run/mcucomms"),
      "Socket path (default is /run/mcucomms)");
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  std::chrono::duration wait_time = std::chrono::milliseconds(500);

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
