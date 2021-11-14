#pragma once

#include <span>
#include <tuple>
#include <utility>

/// \file
/// General purpose utilities. Mostly lambda magic

namespace otto::util {

  inline void set_bit(std::uint8_t& byte, std::size_t idx, bool val)
  {
    byte ^= ((val ? -1 : 0) ^ byte) & (1ul << idx);
  }

  inline void set_bit(std::span<std::uint8_t> array, std::size_t idx, bool val)
  {
    set_bit(array[idx / 8], idx % 8, val);
  }

  inline bool get_bit(std::uint8_t byte, std::size_t idx)
  {
    return (byte & (1ul << idx)) != 0;
  }

  inline bool get_bit(std::span<const std::uint8_t> array, std::size_t idx)
  {
    return get_bit(array[idx / 8], idx % 8);
  }

  template<typename T>
  concept narrowable = std::integral<T> || std::floating_point<T>;

  template<narrowable T>
  struct narrowing {
    T value;
    template<narrowable Res>
    constexpr operator Res() const noexcept
    {
      return static_cast<Res>(value);
    }

#define NARROWING_BINOP(OP)                                                                                            \
  template<narrowable U>                                                                                               \
  friend constexpr U operator OP(const narrowing& l, const U& r) noexcept                                              \
  {                                                                                                                    \
    return static_cast<U>(l.value) OP r;                                                                               \
  }                                                                                                                    \
  template<narrowable U>                                                                                               \
  friend constexpr U operator OP(const U& l, const narrowing& r) noexcept                                              \
  {                                                                                                                    \
    return static_cast<U>(l.value) OP r;                                                                               \
  }

    NARROWING_BINOP(+)
    NARROWING_BINOP(-)
    NARROWING_BINOP(*)
    NARROWING_BINOP(/)
    NARROWING_BINOP(&)
    NARROWING_BINOP(|)
    NARROWING_BINOP(^)
    NARROWING_BINOP(%)
    NARROWING_BINOP(&&)
    NARROWING_BINOP(||)
#undef NARROWING_BINOP

    template<narrowable U>
    constexpr bool operator==(U r)
    {
      return static_cast<U>(value) == r;
    }

    template<narrowable U>
    constexpr auto operator<=>(U r)
    {
      return static_cast<U>(value) <=> r;
    }
  };

  template<narrowable T>
  constexpr narrowing<T> narrow(T value)
  {
    return narrowing<T>{value};
  }

} // namespace otto::util
