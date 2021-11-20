#include "controller.hpp"
#include "lib/i2c.hpp"

#include <fmt/format.h>

#include <thread>

namespace otto {

Controller::Controller(mcu_port_t &&port,
                       std::chrono::duration<double> wait_time)
    : wait_time_(wait_time), port_(std::move(port)),
      thread_([this](const std::stop_token &stop_token) {
        // Start separate thread for reading from MCU
        // This thread is easy to stop because of the polling
        std::jthread child{[this](std::stop_token child_stop_token) {
          util::Packet p;
          while (!child_stop_token.stop_requested()) {
            bool do_continue = true;
            while (do_continue) {
              p = this->port_.read();
              if (p.cmd == util::Command{239}) {
                fmt::print("MCU->RPi: [{}: {}]\n",
                           static_cast<std::uint8_t>(p.cmd),
                           fmt::join(p.data, ", "));
                p.cmd = util::Command::none; // HACK
              }
              if (p.cmd == util::Command::none)
                do_continue = false;
              if (p.cmd == util::Command::shutdown) {
                fmt::print("MCU->RPi: Shutdown!");
                std::system("shutdown");
                do_continue = false;
              }
              if (p.cmd != util::Command::none)
                fmt::print("MCU->RPi: [{}: {}]\n",
                           static_cast<std::uint8_t>(p.cmd),
                           fmt::join(p.data, ", "));
              distribute_packet(p);
            }
            std::this_thread::sleep_for(this->wait_time_);
          }
        }};

        // Write to MCU
        util::Packet data;
        while (!stop_token.stop_requested()) {
          this->queue_.wait_dequeue(data);
          fmt::print("RPi->MCU: [{}: {}]", data.cmd,
                     fmt::join(data.data, ", "));
          this->port_.write(data);
        }
      }) {}

void Controller::distribute_packet(util::Packet &p) {
  for (auto conn : connections_) {
    conn->enqueue(p);
  }
}

} // namespace otto
