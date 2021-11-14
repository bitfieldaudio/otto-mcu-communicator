#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <system_error>
#include <vector>
#include <ranges>
#include <algorithm>

#include<magic_enum.hpp>

#include "lib/utility.hpp"

namespace otto::util {

  struct I2C {
    I2C() = default;
    I2C(std::uint16_t address);
    ~I2C() noexcept;

    bool is_open() const noexcept;
    [[nodiscard]] std::error_code open(const std::filesystem::path&);
    [[nodiscard]] std::error_code close();

    [[nodiscard]] std::error_code write(std::span<const std::uint8_t> message);
    [[nodiscard]] std::error_code read_into(std::span<std::uint8_t> buffer);

  private:
    std::vector<std::uint8_t> buffer;
    int i2c_fd = -1;
    std::uint16_t address = 0;
  };

  enum struct Command : std::uint8_t {
    none = 0,
    leds_buffer = 1,
    leds_commit = 2,
    key_events = 3,
    encoder_events = 4,
    shutdown = 5,
    heartbeat = 6,
  };

  struct Packet {
    Command cmd = Command::none;
    std::array<std::uint8_t, 16> data = {0};

    [[nodiscard]] std::array<std::uint8_t, 17> to_array() const {
      std::array<std::uint8_t, 17> res = {};
      res[0] = magic_enum::enum_integer(cmd);
      std::ranges::copy(data, res.begin() + 1);
      return res;
    }

    bool operator==(const Packet &) const noexcept = default;
  };

} // namespace otto::util
