#pragma once

#include <iostream>
#include <utility>
#include <string>
#include <thread>

#include "lib/i2c.hpp"

struct hex {
  std::uint8_t data;
  friend std::ostream& operator<<(std::ostream& str, const hex& self)
  {
    auto flags = str.flags();
    auto& res = str << std::setfill('0') << std::setw(2) << std::hex << (int) self.data;
    str.flags(flags);
    return res;
  }
};

namespace otto {
  using namespace std::chrono_literals;

  struct I2CMCUPort {
    struct Config {
      std::uint16_t address;
      std::string device_path;
    };

    I2CMCUPort(Config c) : conf(std::move(c)), i2c(conf.address)
    {
      std::cout << "Opening " << conf.device_path << " with address "
                << hex{static_cast<std::uint8_t>(conf.address)} << std::endl;
      auto ec = i2c.open(conf.device_path);
      if (ec) {
        std::cout << "error opening i2c device: " << ec << std::endl;
        throw std::system_error(ec);
      }
    }

    void write(const otto::util::Packet& p)
    {
      // LOGI("Sending packet: {} {:02X}", util::enum_name(p.cmd), fmt::join(p.data, " "));
      std::error_code ec;
      for (int i = 0; i < 10; i++) {
        ec = i2c.write(p.to_array());
        if (ec.value() == EREMOTEIO) {
          // LOGW("MCU nack'd i2c write ({})", i + 1);
          // Back off linearly
          std::this_thread::sleep_for(i * 100ns);
          ec = {};
        } else {
          break;
        }
      }
      if (ec) throw std::system_error(ec);
    }

    otto::util::Packet read()
    {
      std::array<std::uint8_t, 17> data = {};

      std::error_code ec;
      for (int i = 0; i < 10; i++) {
        auto ec = i2c.read_into(data);
        if (ec.value() == EREMOTEIO) {
          // LOGW("MCU nack'd i2c read ({})", i + 1);
          // Back off linearly
          std::this_thread::sleep_for(i * 100ns);
          ec = {};
        } else {
          break;
        }
      }
      if (ec) throw std::system_error(ec);

      if (data[0] == 0) return {};
      otto::util::Packet res = {static_cast<otto::util::Command>(data[0])};
      std::copy(data.begin() + 1, data.end(), res.data.begin());
      return res;
    }

    Config conf;
    util::I2C i2c;
  };


  struct TESTMCUPort {
    struct Config {
      std::uint16_t address;
      std::string device_path;
    };

    TESTMCUPort(Config c) : conf(std::move(c))
    {
      std::cout << "Opening test MCU port" << std::endl;
    }

    void write(const otto::util::Packet& p)
    {
      // Do nothing
    }

    otto::util::Packet read()
    {
      std::array<std::uint8_t, 17> dummy_data = {1};

      if (data_count == 3) {
        data_count = 0;
        return {};
      }

      otto::util::Packet res = {static_cast<otto::util::Command>(dummy_data[0])};
      std::copy(dummy_data.begin() + 1, dummy_data.end(), res.data.begin());
      data_count++;
      return res;
    }

    Config conf;
    int data_count = 0;
  };

} // namespace otto::drivers

using mcu_port_t = otto::TESTMCUPort;

