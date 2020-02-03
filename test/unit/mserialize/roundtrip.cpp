#include "test_enums.hpp"
#include "test_streams.hpp"
#include "test_type_lists.hpp"

#include <mserialize/deserialize.hpp>
#include <mserialize/serialize.hpp>

#include <mserialize/make_struct_deserializable.hpp>
#include <mserialize/make_struct_serializable.hpp>
#include <mserialize/make_template_deserializable.hpp>
#include <mserialize/make_template_serializable.hpp>

#include <boost/optional/optional.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm> // equal
#include <cmath> // isnan
#include <cstddef> // size_t
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <utility> // pair
#include <vector>

namespace {

template <typename In, typename Out>
void roundtrip_into(const In& in, Out& out)
{
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);

  // serialize
  OutputStream ostream{stream};
  mserialize::serialize(in, ostream);

  // make sure computed serialized size is correct
  BOOST_TEST(std::size_t(stream.tellp()) == mserialize::serialized_size(in));

  // deserialize
  InputStream istream{stream};
  mserialize::deserialize(out, istream);
}

template <typename T>
T roundtrip(const T& in)
{
  T out;
  roundtrip_into(in, out);
  return out;
}

// Boost.Test has trouble comparing/printing nested containers
template <typename A, typename B, typename Cmp>
bool deep_container_equal(const A& a, const B& b, Cmp cmp)
{
  using std::begin;
  using std::end;
  return std::equal(begin(a), end(a), begin(b), end(b), cmp);
}

auto container_equal()
{
  return [](auto& a, auto& b)
  {
    using std::begin;
    using std::end;
    BOOST_TEST(a == b, boost::test_tools::per_element());
    return std::equal(begin(a), end(a), begin(b), end(b));
  };
}

template <typename T>
bool pointee_equal(const T* a, const T* b)
{
  return (a && b) ? *a == *b : !a && !b;
}

} // namespace

// specialization required by the optional tests

namespace mserialize {
namespace detail {

template <typename T>
struct is_optional<boost::optional<T>> : std::true_type {};

} // namespace detail
} // namespace mserialize

BOOST_AUTO_TEST_SUITE(MserializeRoundtrip)

BOOST_AUTO_TEST_CASE_TEMPLATE(arithmetic_min_max, T, arithmetic_types)
{
  // min
  {
    const T in = std::numeric_limits<T>::max();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // max
  {
    const T in = std::numeric_limits<T>::min();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(float_spec, T, float_types)
{
  // lowest
  {
    const T in = std::numeric_limits<T>::lowest();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // Negative 0
  {
    const T in{-0.0};
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // -Inf
  if (std::numeric_limits<T>::has_infinity)
  {
    const T in = T{-1.} * std::numeric_limits<T>::infinity();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // +Inf
  if (std::numeric_limits<T>::has_infinity)
  {
    const T in = std::numeric_limits<T>::infinity();
    const T out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // Quiet NaN
  if (std::numeric_limits<T>::has_quiet_NaN)
  {
    const T in = std::numeric_limits<T>::quiet_NaN();
    const T out = roundtrip(in);
    BOOST_TEST(std::isnan(out));
  }

  // Signaling NaN
  if (std::numeric_limits<T>::has_signaling_NaN)
  {
    const T in = std::numeric_limits<T>::signaling_NaN();
    const T out = roundtrip(in);
    BOOST_TEST(std::isnan(out));
  }
}

// is_sequence_batch_serializable

static_assert(mserialize::detail::is_sequence_batch_serializable<std::vector<int>>::value, "");
static_assert(mserialize::detail::is_sequence_batch_serializable<std::string>::value, "");
static_assert(mserialize::detail::is_sequence_batch_serializable<std::array<bool, 16>>::value, "");
static_assert(mserialize::detail::is_sequence_batch_serializable<int(&)[8]>::value, "");

static_assert(! mserialize::detail::is_sequence_batch_serializable<std::vector<std::vector<int>>>::value, "");
static_assert(! mserialize::detail::is_sequence_batch_serializable<std::array<std::string, 16>>::value, "");
static_assert(! mserialize::detail::is_sequence_batch_serializable<std::vector<int>(&)[8]>::value, "");

// is_sequence_batch_deserializable

static_assert(mserialize::detail::is_sequence_batch_deserializable<std::vector<int>>::value, "");
static_assert(mserialize::detail::is_sequence_batch_deserializable<std::array<bool, 16>>::value, "");
static_assert(mserialize::detail::is_sequence_batch_deserializable<int(&)[8]>::value, "");

static_assert(! mserialize::detail::is_sequence_batch_deserializable<std::vector<std::vector<int>>>::value, "");
static_assert(! mserialize::detail::is_sequence_batch_deserializable<std::array<std::string, 16>>::value, "");
static_assert(! mserialize::detail::is_sequence_batch_deserializable<std::vector<int>(&)[8]>::value, "");

BOOST_AUTO_TEST_CASE_TEMPLATE(sequence_of_int, T, sequence_types<int>)
{
  /*const*/ T in{0,1,2,3,4,5,6,7,8,9};
  T out; // NOLINT(cppcoreguidelines-pro-type-member-init)
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(empty_sequence_of_int, T, var_size_sequence_types<int>)
{
  /*const*/ T in;
  T out{1, 2, 3};
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(sequence_of_vector_of_int, T, sequence_types<std::vector<int>>)
{
  using V = std::vector<int>;
  const T in{
    V{}, V{1,2,3}, V{4,5,6},
    V{7}, V{8,9}, V{10,11,12,13,14,15,16},
    V{17,18,19,20}, V{21,21}, V{22}, V{}
  };
  T out;
  roundtrip_into(in, out);

  BOOST_TEST(deep_container_equal(
    in, out, container_equal()
  ));
}

BOOST_AUTO_TEST_CASE(sequence_cross)
{
  const std::vector<std::deque<std::array<int, 3>>> in{
    { {1,2,3}, {4,5,6} },
    { {7,8,9}          },
    { {10, 11, 12}, {13,14,15}, {16,17,18}}
  };

  std::forward_list<std::list<int>> out[3];
  roundtrip_into(in, out);

  BOOST_TEST(deep_container_equal(
    in, out,
    [](auto& c, auto& d)
    {
      return deep_container_equal(c, d, container_equal());
    }
  ));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set, T, sets<int>)
{
  const T in{1, 2, 7, 7, 7, 9, 2, 8};
  const T out = roundtrip(in);
  BOOST_TEST(in == out);
}

// Can't pass argument to BOOST macro with commas inside
using IntCharMaps = maps<int, char>;

BOOST_AUTO_TEST_CASE_TEMPLATE(map, T, IntCharMaps)
{
  const T in{ {1, 'a'}, {7, 'x'}, {2, 'b'}, {4, 'y'}, {7, 'z'} };
  const T out = roundtrip(in);
  BOOST_TEST(in == out);
}

// Can't pass argument to BOOST macro with commas inside
using CharIntTuple = std::tuple<char, int>;

BOOST_AUTO_TEST_CASE_TEMPLATE(sequence_of_tuples, T, sequence_types<CharIntTuple>)
{
  const T in{
    CharIntTuple{'1',2}, CharIntTuple{'3',4}, CharIntTuple{'5',6},
    CharIntTuple{'7',8}, CharIntTuple{'9',10}, CharIntTuple{'A',12},
    CharIntTuple{'C',14}, CharIntTuple{'E',15}, CharIntTuple{'G',17}, CharIntTuple{'I',19}
  };
  T out;
  roundtrip_into(in, out);
  BOOST_TEST(std::equal(begin(in), end(in), begin(out), end(out)));
}

BOOST_AUTO_TEST_CASE(vector_of_bool)
{
  const std::vector<bool> in{true, false, false, true, true, false};
  std::vector<bool> out{false, false};
  roundtrip_into(in, out);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(sequence_size_mismatch)
{
  const std::array<int, 3> in{1,2,3};
  std::array<int, 6> out{0,0,0,0,0,0};
  BOOST_CHECK_THROW(
    roundtrip_into(in, out),
    std::runtime_error
  );
}

BOOST_AUTO_TEST_CASE(string)
{
  // empty
  {
    const std::string in;
    const std::string out = roundtrip(in);
    BOOST_TEST(in == out);
  }

  // not-empty
  {
    const std::string in = "foobar";
    const std::string out = roundtrip(in);
    BOOST_TEST(in == out);
  }
}

BOOST_AUTO_TEST_CASE(tuples)
{
  // empty
  {
    const std::tuple<> in;
    const std::tuple<> out = roundtrip(in);
    BOOST_TEST((in == out));
  }

  // single
  {
    const std::tuple<int> in{123};
    const std::tuple<int> out = roundtrip(in);
    BOOST_TEST((in == out));
  }

  // two, one of a kind
  {
    const std::tuple<std::int16_t, std::int16_t> in{456, 789};
    const std::tuple<std::int16_t, std::int16_t> out = roundtrip(in);
    BOOST_TEST((in == out));
  }

  // two nested
  {
    const std::tuple<
      std::tuple<int, int>,
      std::tuple<int, int>
    > in{std::tuple<int, int>{1,2}, std::tuple<int, int>{3,4}};
    const auto out = roundtrip(in);
    BOOST_TEST((in == out));
  }

  // more, mixed
  {
    const std::tuple<
      int, std::vector<int>, std::pair<std::int16_t, std::int16_t>,
      std::deque<char>
    > in{1, {2,3,4}, {5,6}, {'7','8','9'}};
    const auto out = roundtrip(in);
    BOOST_TEST((in == out));
  }
}

BOOST_AUTO_TEST_CASE(pairs)
{
  // two, one of a kind
  {
    const std::pair<int, int> in{1,2};
    const std::pair<int, int> out = roundtrip(in);
    BOOST_TEST((in == out));
  }

  // nested
  {
    const std::pair<
      std::pair<char, std::int16_t>,
      std::pair<int, std::int64_t>
    > in{{'1',2}, {3,4}};
    const auto out = roundtrip(in);
    BOOST_TEST((in == out));
  }
}

BOOST_AUTO_TEST_CASE(tuple_pair_cross)
{
  // tuple -> pair
  {
    const std::tuple<int, float> in{1,1.0f};
    std::pair<int, float> out;
    roundtrip_into(in, out);
    BOOST_TEST(std::get<0>(in) == std::get<0>(out));
    BOOST_TEST(std::get<1>(in) == std::get<1>(out));
  }

  // pair -> tuple
  {
    const std::pair<int, char> in{1,'2'};
    std::tuple<int, char> out;
    roundtrip_into(in, out);
    BOOST_TEST(std::get<0>(in) == std::get<0>(out));
    BOOST_TEST(std::get<1>(in) == std::get<1>(out));
  }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(smart_pointer, T, smart_pointers<int>)
{
  // empty
  {
    const T in;
    const T out = roundtrip(in);
    BOOST_TEST(!out);
  }

  // not empty
  {
    const T in(new int(123));
    const T out = roundtrip(in);
    BOOST_TEST_REQUIRE(!!out);
    BOOST_TEST(*in == *out);
  }
}

BOOST_AUTO_TEST_CASE(pointers)
{
  const int value = 456;
  const int* in = &value;
  std::unique_ptr<int> out;
  roundtrip_into(in, out);
  BOOST_TEST_REQUIRE(!!out);
  BOOST_TEST(*in == *out);

  static_assert(
    mserialize::detail::negation<
      mserialize::detail::is_deserializable<int*>
    >::value, "Deserialization into raw pointers is not supported to avoid leaks"
  );
}

BOOST_AUTO_TEST_CASE_TEMPLATE(nested_smart_pointers, T, smart_pointers<std::unique_ptr<int>>)
{
  const T in(new std::unique_ptr<int>(new int(123)));
  const T out = roundtrip(in);

  BOOST_TEST_REQUIRE(!!out);
  BOOST_TEST_REQUIRE(!!*out);
  BOOST_TEST(**out == 123);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(mixed_smart_pointers, T, smart_pointers<std::vector<std::tuple<int>>>)
{
  const std::vector<std::tuple<int>> value{
    std::tuple<int>{1}, std::tuple<int>{2},
    std::tuple<int>{3}, std::tuple<int>{4},
  };
  const T in(new std::vector<std::tuple<int>>(value));
  const T out = roundtrip(in);
  BOOST_TEST_REQUIRE(!!out);
  BOOST_TEST((*out == value));
}

BOOST_AUTO_TEST_CASE(optional)
{
  // empty
  {
    const boost::optional<int> in;
    boost::optional<int> out(123);
    roundtrip_into(in, out);
    BOOST_TEST(!out);
  }

  // not empty
  {
    const boost::optional<int> in(123);
    const boost::optional<int> out = roundtrip(in);
    BOOST_TEST_REQUIRE(!!out);
    BOOST_TEST(*out == *in);
  }

  // mixed
  {
    using Value = std::vector<std::tuple<int>>;
    const boost::optional<Value> in(Value{
      std::tuple<int>{1}, std::tuple<int>{2},
      std::tuple<int>{3}, std::tuple<int>{4},
    });
    const boost::optional<Value> out = roundtrip(in);
    BOOST_TEST_REQUIRE(!!out);
    BOOST_TEST((*out == *in));
  }
}

BOOST_AUTO_TEST_CASE(cenum)
{
  const test::CEnum in{test::Alpha};
  const test::CEnum out = roundtrip(in);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(enumClass)
{
  const test::EnumClass in{test::EnumClass::Echo};
  const test::EnumClass out = roundtrip(in);
  BOOST_TEST((in == out));
}

BOOST_AUTO_TEST_CASE(largeEnumClass)
{
  const test::LargeEnumClass in{test::LargeEnumClass::Golf};
  const test::LargeEnumClass out = roundtrip(in);
  BOOST_TEST((in == out));
}

BOOST_AUTO_TEST_CASE(errorOnEof)
{
  int out;
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);
  BOOST_CHECK_THROW(
    mserialize::deserialize(out, stream),
    std::exception
  );
}

BOOST_AUTO_TEST_CASE(errorOnIncomplete)
{
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);
  mserialize::serialize(std::int16_t{123}, stream);

  std::int32_t out;
  BOOST_CHECK_THROW(
    mserialize::deserialize(out, stream),
    std::exception
  );
}

BOOST_AUTO_TEST_SUITE_END()

struct Person
{
  int age = 0;
  std::string name;
};

bool operator==(const Person& a, const Person& b)
{
  return a.age == b.age && a.name == b.name;
}

std::ostream& operator<<(std::ostream& out, const Person& p)
{
  return out << "Person{ age: " << p.age << ", name: " << p.name << " }";
}

namespace test {

struct NsPerson
{
  int age = 0;
  std::string name;

  friend bool operator==(const NsPerson& a, const NsPerson& b)
  {
    return a.age == b.age && a.name == b.name;
  }
};

} // namespace test

struct Vehicle
{
  int type = 0;
  int _age = 0;
  std::string _name;
  std::unique_ptr<Person> _owner;

  int age() const { return _age; }
  int age(const int i) { return _age = i; }

  std::string name() const { return _name; }
  void name(const std::string& n) { _name = n; }

  // Limitation: [de]serializable_member is unable
  // to remove this from the overload set.
  //template <typename T>
  //std::string name(T = 0) const { return _name; }

private:
  const std::unique_ptr<Person>& owner() const { return _owner; }
  bool owner(std::unique_ptr<Person> o) { _owner = std::move(o); return !!_owner; }

  template <typename, typename>
  friend struct mserialize::CustomSerializer;

  template <typename, typename>
  friend struct mserialize::CustomDeserializer;

  friend bool operator==(const Vehicle& a, const Vehicle& b)
  {
    return a.type == b.type
      &&   a.age() == b.age()
      &&   a.name() == b.name()
      &&   pointee_equal(a.owner().get(), b.owner().get());
  }

  friend std::ostream& operator<<(std::ostream& out, const Vehicle& v)
  {
    out << "Vehicle{"
        << " type: " << v.type
        << ", age: " << v.age()
        << ", name: " << v.name()
        << ", owner: ";
    if (v.owner()) { out << *v.owner(); }
    else { out << "{null}"; }
    return out << " }";
  }
};

template <typename A, typename B>
struct Pair
{
  A a; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  B b() const { return _b; }
  void b(B b) { _b = b; }

  Pair() = default; // NOLINT(cppcoreguidelines-pro-type-member-init)
  Pair(A a, B b) :a(std::move(a)), _b(std::move(b)) {}

private:
  B _b;

  friend bool operator==(const Pair& a, const Pair& b)
  {
    return a.a == b.a && a._b == b._b;
  }

  friend std::ostream& operator<<(std::ostream& out, const Pair& p)
  {
    return out << "Pair{"
               << " a: " << p.a
               << ", b: " << p.b()
               << " }";
  }
};

namespace test {

template <typename A, typename B>
struct NsPair
{
  A a{};
  B b{};

  friend bool operator==(const NsPair& a, const NsPair& b)
  {
    return a.a == b.a && a.b == b.b;
  }
};

} // namespace test

template <typename T, std::size_t N>
struct Array
{
  std::array<T, N> a;

private:
  friend bool operator==(const Array& a, const Array& b)
  {
    return a.a == b.a;
  }
};

namespace mserialize {

template <>
struct CustomSerializer<Person>
{
  static void serialize(const Person& p, OutputStream& ostream)
  {
    // make sure custom serializers can use their own schema
    ostream.write("foobar", 6);
    mserialize::serialize(p.age, ostream);
    mserialize::serialize(p.name, ostream);
  }

  static std::size_t serialized_size(const Person& p)
  {
    return 6 + mserialize::serialized_size(p.age) + mserialize::serialized_size(p.name);
  }
};

template <>
struct CustomDeserializer<Person>
{
  static void deserialize(Person& p, InputStream& istream)
  {
    char buffer[6] = {0};
    istream.read(buffer, 6);
    const std::string foobar(buffer, 6);
    if (foobar != "foobar")
    {
      throw std::runtime_error("Invalid magic: " + foobar);
    }

    mserialize::deserialize(p.age, istream);
    mserialize::deserialize(p.name, istream);
  }
};

} // namespace mserialize

MSERIALIZE_MAKE_STRUCT_SERIALIZABLE(Vehicle, type, age, name, owner)
MSERIALIZE_MAKE_STRUCT_DESERIALIZABLE(Vehicle, type, age, name, owner)

MSERIALIZE_MAKE_STRUCT_SERIALIZABLE(test::NsPerson, age, name)
MSERIALIZE_MAKE_STRUCT_DESERIALIZABLE(test::NsPerson, age, name)

MSERIALIZE_MAKE_TEMPLATE_SERIALIZABLE((typename A, typename B), (Pair<A,B>), a, b)
MSERIALIZE_MAKE_TEMPLATE_DESERIALIZABLE((typename A, typename B), (Pair<A,B>), a, b)

MSERIALIZE_MAKE_TEMPLATE_SERIALIZABLE((typename A, typename B), (test::NsPair<A,B>), a, b)
MSERIALIZE_MAKE_TEMPLATE_DESERIALIZABLE((typename A, typename B), (test::NsPair<A,B>), a, b)

MSERIALIZE_MAKE_TEMPLATE_SERIALIZABLE((typename T, std::size_t N), (Array<T,N>), a)
MSERIALIZE_MAKE_TEMPLATE_DESERIALIZABLE((typename T, std::size_t N), (Array<T,N>), a)

BOOST_AUTO_TEST_SUITE(MserializeRoundtripCustom)

BOOST_AUTO_TEST_CASE(manual_specialization)
{
  const Person in{33, "John"};
  const Person out = roundtrip(in);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(derived_specialization)
{
  const Vehicle in{1964, 55, "Car", std::make_unique<Person>(Person{35, "Ferdinand"})};
  const Vehicle out = roundtrip(in);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(namespaced_specialization)
{
  const test::NsPerson in{27, "Juliet"};
  const test::NsPerson out = roundtrip(in);
  BOOST_TEST((in == out));
}

BOOST_AUTO_TEST_CASE(template_specialization)
{
  const Pair<int, std::string> in{123, "foobar"};
  const Pair<int, std::string> out = roundtrip(in);
  BOOST_TEST(in == out);
}

BOOST_AUTO_TEST_CASE(namespaced_template_specialization)
{
  const test::NsPair<int, std::string> in{456, "barbaz"};
  const test::NsPair<int, std::string> out = roundtrip(in);
  BOOST_TEST((in == out));
}

BOOST_AUTO_TEST_CASE(template_with_value_args)
{
  const Array<int, 3> in{{1,2,3}};
  Array<int, 3> out{{0,0,0}};
  roundtrip_into(in, out);
  BOOST_TEST((in == out));
}

BOOST_AUTO_TEST_SUITE_END()
