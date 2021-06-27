#ifndef MSERIALIZE_MAKE_ENUM_TAG_HPP
#define MSERIALIZE_MAKE_ENUM_TAG_HPP

#include <mserialize/cx_string.hpp>
#include <mserialize/detail/foreach.hpp>
#include <mserialize/detail/integer_to_hex.hpp>
#include <mserialize/detail/preprocessor.hpp>
#include <mserialize/detail/type_traits.hpp>
#include <mserialize/tag.hpp>

/**
 * MSERIALIZE_MAKE_ENUM_TAG(Enum, enumerators...)
 *
 * Define a CustomTag specialization for the given Enum,
 * allowing visitation of serialized enums.
 *
 * The macro has to be called in global scope
 * (outside of any namespace).
 * Works with regular and scoped enums (enum class).
 * The list of enumerators can be empty.
 *
 * Example:
 *
 *     enum class Flag { A, B, C};
 *     MSERIALIZE_MAKE_ENUM_TAG(Flag, A, B, C)
 *
 * If an enumerator is omitted from the macro call,
 * the tag will be incomplete, and during visitation,
 * if the missing enumerator is visited,
 * only its underlying value will be available,
 * the enumerator name will be empty.
 *
 * The maximum number of enumerators is limited by
 * mserialize/detail/foreach.hpp, currently 100.
 */
#define MSERIALIZE_MAKE_ENUM_TAG(...)                          \
  namespace mserialize {                                       \
  template <>                                                  \
  struct CustomTag<typename ::MSERIALIZE_FIRST(__VA_ARGS__)>   \
  {                                                            \
    using Enum = detail::type_identity<                        \
      typename ::MSERIALIZE_FIRST(__VA_ARGS__)                 \
    >::type;                                                   \
    using underlying_t = std::underlying_type_t<Enum>;         \
                                                               \
    static constexpr auto tag_string()                         \
    {                                                          \
      return cx_strcat(                                        \
        make_cx_string("/"),                                   \
        tag<underlying_t>(),                                   \
        make_cx_string("`" MSERIALIZE_STRINGIZE(MSERIALIZE_FIRST(__VA_ARGS__)) "'"), \
        MSERIALIZE_FOREACH(MSERIALIZE_ENUMERATOR, _, __VA_ARGS__)                    \
        make_cx_string("\\")                                   \
      );                                                       \
    }                                                          \
  };                                                           \
  } /* namespace mserialize */                                 \
  /**/

#define MSERIALIZE_ENUMERATOR(_, enumerator)                   \
  mserialize::detail::integer_to_hex<underlying_t, static_cast<underlying_t>(Enum::enumerator)>(), \
  make_cx_string("`" MSERIALIZE_STRINGIZE(enumerator) "'"),    \
  /**/

#endif // MSERIALIZE_MAKE_ENUM_TAG_HPP
