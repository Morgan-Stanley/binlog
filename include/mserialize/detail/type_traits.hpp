#ifndef MSERIALIZE_DETAIL_TYPE_TRAITS_HPP
#define MSERIALIZE_DETAIL_TYPE_TRAITS_HPP

#include <tuple>
#include <type_traits>
#include <utility> // pair

namespace mserialize {
namespace detail {

// void_t

// C++17: template <typename...> using void_t = void;
// A different implementation is here to protect against CWG 1558
template <typename... Ts> struct make_void { using type = void; };
template <typename... Ts> using void_t = typename make_void<Ts...>::type;

// enable_spec_if

template <typename Pred>
using enable_spec_if = void_t<std::enable_if_t<Pred::value, void>>;

// always_false

// Using this in static_assert does not violate
// [temp.res]/8 "If no valid specialization can be generated ... the template is ill-formed",
// as it is theoretically possible to specialize this helper, e.g:
// template <> struct always_false<X> : std::true_type {};
template <typename>
struct always_false : std::false_type {};

// negation (C++17)

template <typename B>
struct negation : std::integral_constant<bool, !B::value> {};

// conjunction (C++17)

template <typename...>
struct conjunction : std::true_type {};

template <typename B>
struct conjunction<B> : B {};

template <typename B, typename... Bn>
struct conjunction<B, Bn...>
  : std::conditional_t<B::value, conjunction<Bn...>, B> {};

// remove_cvref_t (C++20)

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// Is serializable

template <typename T>
struct Serializer;

template <typename T>
using is_serializable = std::is_constructible<typename Serializer<T>::type>;

// Is deserializable

template <typename T>
struct Deserializer;

template <typename T>
using is_deserializable = std::is_constructible<typename Deserializer<T>::type>;

// is_tuple

template <typename T>
struct is_tuple : std::false_type {};

template <typename... T>
struct is_tuple<std::tuple<T...>> : std::true_type {};

template <typename T, typename U>
struct is_tuple<std::pair<T, U>> : std::true_type {};

// is_optional

template <typename T, typename = void>
struct is_optional : std::is_pointer<T> {};

template <typename T>
struct is_optional<T, void_t<
  typename T::element_type,
  decltype(bool(std::declval<T>())), // can't use is_convertible, explicit conversion is enough
  decltype(*std::declval<T>())
>> : std::true_type {};

// deep_remove_const - const std::pair<const int, const bool> -> std::pair<int, bool>

template <typename T>
struct deep_remove_const : std::remove_const<T> {};

template <template <typename...> class T, typename... E>
struct deep_remove_const<T<E...>>
{
  using type = std::remove_const_t<T<typename deep_remove_const<E>::type...>>;
};

template <template <typename...> class T, typename... E>
struct deep_remove_const<const T<E...>> : deep_remove_const<T<E...>> {};

template <typename T>
using deep_remove_const_t = typename deep_remove_const<T>::type;

// type_identity (c++20)

template <typename T>
struct type_identity { using type = T; };

} // namespace detail
} // namespace mserialize

#endif // MSERIALIZE_DETAIL_TYPE_TRAITS_HPP
