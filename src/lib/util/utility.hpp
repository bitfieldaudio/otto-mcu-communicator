#pragma once

#include <span>
#include <tuple>
#include <utility>

#include <tl/optional.hpp>

#include "lib/util/macros.hpp"
#include "lib/util/string_ref.hpp"

/// \file
/// General purpose utilities. Mostly lambda magic

namespace otto::util {
  namespace utility_detail {
    template<typename T>
    struct function_ptr_impl;
    template<typename Ret, typename... Args>
    struct function_ptr_impl<Ret(Args...)> {
      using type = Ret (*)(Args...);
    };
    template<typename Ret, typename... Args>
    struct function_ptr_impl<Ret(Args...) noexcept> {
      using type = Ret (*)(Args...) noexcept;
    };
  } // namespace utility_detail

  template<typename Func>
  using function_ptr = typename utility_detail::function_ptr_impl<Func>::type;

  template<typename Class, typename Ret, typename... Args>
  using member_func_ptr = Ret (Class::*)(Args...);

  template<typename Class, typename T>
  using member_ptr = T Class::*;

  // overloaded ///////////////////////////////////////////////////////////////

  /// Overload lambdas
  template<typename... Ls>
  struct overloaded : Ls... {
    overloaded(Ls... ls) : Ls(ls)... {}
    using Ls::operator()...;
  };

  template<typename... Ls>
  overloaded(Ls...) -> overloaded<std::decay_t<Ls>...>;

  // match ////////////////////////////////////////////////////////////////////

  /// Pattern matching for std::variant.
  ///
  /// One of `Lambdas` is required to match
  template<typename Var, typename... Lambdas>
  decltype(auto) match(Var&& v, Lambdas&&... ls)
  {
    auto&& matcher = overloaded(std::forward<Lambdas>(ls)...);
    // ADL to use std::visit or mpark::visit
    // TODO: Remove this when the standard is adapted
    return visit(std::move(matcher), std::forward<Var>(v));
  }

  /// Match any type, do nothing with it
  const constexpr auto nullmatch = [](auto&&) {};

  /// Pattern matching for std::variant.
  ///
  /// Does not require all states of `v` to be matched, and defaults to no
  /// action.
  template<typename Var, typename... Lambdas>
  decltype(auto) partial_match(Var&& v, Lambdas&&... ls)
  {
    return match(std::forward<Var>(v), std::forward<Lambdas>(ls)..., nullmatch);
  }

  // capture_this /////////////////////////////////////////////////////////////

  /// Get a callable from a member pointer and an object.
  template<typename Type, typename Ret, typename... Args>
  auto capture_this(Ret (Type::*func)(Args...), Type* object)
  {
    return [object, func](Args... args) -> Ret { return (object->*func)(args...); };
  }
  template<typename Type, typename Ret, typename... Args>
  auto capture_this(Ret (Type::*func)(Args...), Type& object)
  {
    return [&object, func](Args... args) -> Ret { return (object.*func)(args...); };
  }

  template<typename T>
  auto does_equal(T&& obj)
  {
    return [obj = std::forward<T>(obj)](auto&& lhs) { return lhs == obj; };
  }

  // Tuple for_each ///////////////////////////////////////////////////////////


  template<typename T>
  struct is_tuple : std::false_type {};
  template<typename... Args>
  struct is_tuple<std::tuple<Args...>> : std::true_type {};

  template<typename T>
  concept ATupleRef = is_tuple<std::remove_cvref_t<T>>::value;

  namespace details {
    template<ATupleRef T, typename F, std::size_t... Is>
    void tuple_for_each_impl(T&& t, F&& f, std::integer_sequence<std::size_t, Is...> is)
    {
      (f(std::get<Is>(t)), ...);
    }
    template<ATupleRef T, typename F, std::size_t... Is>
    void tuple_reverse_for_each_impl(T&& t, F&& f, std::integer_sequence<std::size_t, Is...> is)
    {
      (f(std::get<std::tuple_size<std::decay_t<T>>() - 1 - Is>(t)), ...);
    }

    template<ATupleRef T, typename F, std::size_t... Is>
    void tuple_for_each_i_impl(T&& t, F&& f, std::integer_sequence<std::size_t, Is...> is)
    {
      (f(Is, std::get<Is>(t)), ...);
    }

    template<ATupleRef T, typename F, std::size_t... Is>
    auto tuple_transform_impl(T&& t, F&& f, std::integer_sequence<std::size_t, Is...> is)
    {
      return std::forward_as_tuple(f(std::get<Is>(t))...);
    }

    template<ATupleRef T1, ATupleRef T2, std::size_t... Is>
    auto tuple_zip_impl(T1&& t1, T2&& t2, std::integer_sequence<std::size_t, Is...> is)
    {
      return std::tuple(
        std::pair<std::tuple_element_t<Is, std::remove_cvref_t<T1>>, std::tuple_element_t<Is, std::remove_cvref_t<T2>>>(
          std::get<Is>(t1), std::get<Is>(t2))...);
    }

    template<typename F, std::size_t... Is>
    decltype(auto) apply_idxs_impl(F&& f, std::integer_sequence<std::size_t, Is...> is)
    {
      return FWD(f)(Is...);
    }

  } // namespace details

  template<typename F, ATupleRef Tuple>
  void for_each(Tuple&& tuple, F&& f)
  {
    details::tuple_for_each_impl(FWD(tuple), FWD(f),
                                 std::make_index_sequence<std::tuple_size<std::remove_cvref_t<Tuple>>::value>());
  }

  template<typename F, ATupleRef Tuple>
  void reverse_for_each(Tuple&& tuple, F&& f)
  {
    details::tuple_reverse_for_each_impl(
      FWD(tuple), FWD(f), std::make_index_sequence<std::tuple_size<std::remove_cvref_t<Tuple>>::value>());
  }

  /// Call `f(idx, element)` for each `element` in `tuple`
  template<typename F, ATupleRef Tuple>
  void indexed_for_each(Tuple&& tuple, F&& f)
  {
    details::tuple_for_each_i_impl(FWD(tuple), FWD(f),
                                   std::make_index_sequence<std::tuple_size<std::remove_cvref_t<Tuple>>::value>());
  }

  template<typename F, ATupleRef Tuple>
  auto transform(Tuple&& tuple, F&& f)
  {
    return details::tuple_transform_impl(
      FWD(tuple), FWD(f), std::make_index_sequence<std::tuple_size<std::remove_cvref_t<Tuple>>::value>());
  }

  template<typename F, ATupleRef Tuple>
  decltype(auto) apply_idxs(Tuple&& tuple, F&& f)
  {
    return std::apply(FWD(f), std::make_index_sequence<std::tuple_size<std::remove_cvref_t<Tuple>>::value>());
  }

  template<ATupleRef T1, ATupleRef T2>
  auto zip(T1&& t1, T2&& t2)
  {
    return details::tuple_zip_impl(
      FWD(t1), FWD(t2),
      std::make_index_sequence<std::min(std::tuple_size<std::remove_cvref_t<T1>>::value,
                                        std::tuple_size<std::remove_cvref_t<T2>>::value)>());
  }

  namespace detail {
    template<class X>
    constexpr auto remove_last_impl(X&& x)
    {
      return std::tuple<>();
    }

    template<class X, class... Xs>
    constexpr auto remove_last_impl(X&& x, Xs&&... xs);

    constexpr auto tuple_remove_last = [](auto&&... args) { return remove_last_impl(FWD(args)...); };

    template<class X, class... Xs>
    constexpr auto remove_last_impl(X&& x, Xs&&... xs)
    {
      return std::tuple_cat(std::tuple<X>(FWD(x)), std::apply(tuple_remove_last, std::tuple<Xs...>(FWD(xs)...)));
    }
  } // namespace detail

  template<class T, class... Args>
  constexpr auto remove_last(const std::tuple<T, Args...>& t)
  {
    return std::apply(detail::tuple_remove_last, t);
  }

  template<typename T>
  struct tag_t {};

  template<typename T>
  constexpr auto tag = tag_t<T>();

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

  template<typename T>
  tl::optional<T&> opt_ref(tl::optional<T>& opt)
  {
    if (opt) return tl::optional<T&>(*opt);
    return tl::nullopt;
  }

  // Lambdas matching overload sets

  /// std::addressof wrapped in a lambda so it can be passed to algorithms
  constexpr auto addressof = LAMBDAFY(std::addressof);

} // namespace otto::util
