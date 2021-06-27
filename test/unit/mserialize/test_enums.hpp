#ifndef TEST_UNIT_MSERIALIZE_TEST_ENUMS_HPP
#define TEST_UNIT_MSERIALIZE_TEST_ENUMS_HPP

#include <cstdint>

namespace test {

enum CEnum : int
{
  Alpha,
  Bravo,
  Charlie
};

enum class EnumClass : int
{
  Delta,
  Echo,
  Foxtrot
};

enum class LargeEnumClass : std::int64_t
{
  Golf = INT64_MIN,
  Hotel = -1024,
  India = 0,
  Juliet = 2048,
  Kilo = INT64_MAX
};

enum class UnsignedLargeEnumClass : std::uint64_t
{
  Lima = 0,
  Mike = 1024,
  November = 16384,
  Oscar = UINT64_MAX
};

struct EnumNest
{
  enum class Nested
  {
    Bird
  } Nested; // field name and enum name are the same
};

typedef enum : int {
  Papa
} UnnamedEnumTypedef;

} // namespace test

enum UnscopedEnum : int {
  Quebec
};

#endif // TEST_UNIT_MSERIALIZE_TEST_ENUMS_HPP
