#pragma once

#include <chrono>
#include <set>
#include <thread>

#include <blockingconcurrentqueue.h>
#include <readerwriterqueue.h>

#include "app/mcu_port.hpp"

namespace otto {

struct Controller {

  explicit Controller(mcu_port_t &&port,
                      std::chrono::duration<double> wait_time);

  Controller(const Controller &) = delete;
  Controller &operator=(const Controller &) = delete;

  ~Controller() noexcept {}

  auto port_writer() noexcept {
    return [this](const util::Packet &p) { queue_.enqueue(p); };
  }

  moodycamel::BlockingConcurrentQueue<util::Packet> &get_queue() {
    return queue_;
  }

  void add_connection(moodycamel::BlockingReaderWriterQueue<util::Packet> *c) {
    connections_.insert(c);
  }

  void
  remove_connection(moodycamel::BlockingReaderWriterQueue<util::Packet> *c) {
    auto it = connections_.find(c);
    if (it != connections_.end())
      connections_.erase(it);
  }

  void distribute_packet(otto::util::Packet &p);

private:
  mcu_port_t port_;
  std::chrono::duration<double> wait_time_;

  // The queues for MCU -> Clients
  std::set<moodycamel::BlockingReaderWriterQueue<util::Packet> *> connections_;

  // The queue for Clients -> MCU
  moodycamel::BlockingConcurrentQueue<util::Packet> queue_;
  std::jthread thread_;
};

} // namespace otto
