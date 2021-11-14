#include "controller.hpp"

#include <thread>

namespace otto {

Controller::Controller(mcu_port_t &&port,
                       std::chrono::duration<double> wait_time)
    : wait_time_(wait_time), port_(std::move(port)),
      thread_([this](const std::stop_token &stop_token) {


        // Start separate thread for reading from MCU
        // This thread is easy to stop because of the polling
        std::jthread child{[this](std::stop_token child_stop_token){
          util::Packet p;
          while (!child_stop_token.stop_requested()) {
            do {
              p = this->port_.read();
              distribute_packet(p);
            } while (p.cmd != util::Command::none);
            std::this_thread::sleep_for(this->wait_time_);
          }
        }};

        // Write to MCU
        util::Packet data;
        while (!stop_token.stop_requested()) {
          this->queue_.wait_dequeue(data);
          this->port_.write(data);
        }
      }) {}

void Controller::distribute_packet(util::Packet &p) {
  for (auto conn : connections_) {
    conn->enqueue(p);
  }
}

} // namespace otto
