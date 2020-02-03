#include "test_enums.hpp"
#include "test_streams.hpp"
#include "test_type_lists.hpp"

#include <mserialize/make_enum_tag.hpp>
#include <mserialize/serialize.hpp>
#include <mserialize/tag.hpp>
#include <mserialize/visit.hpp>

#include <mserialize/make_struct_serializable.hpp>
#include <mserialize/make_struct_tag.hpp>

#include <boost/test/unit_test.hpp>

#include <sstream>

namespace {

template <typename T>
class StoreT
{
  T _value = {};
  bool _has_visit = false;

public:
  using value_type = T;

  void visit(T t)
  {
    BOOST_TEST(! _has_visit);
    _has_visit = true;
    _value = std::move(t);
  }

  template <typename U>
  void visit(const U&) { BOOST_FAIL("Unexpected visit of U"); }

  T value()
  {
    BOOST_TEST(_has_visit);
    return std::move(_value);
  }
};

class ToString
{
  std::stringstream _str;

public:
  using value_type = std::string;

  ToString() { _str << std::boolalpha; }

  // catch all for arithmetic types
  template <typename T>
  void visit(T v) { _str << v << ' '; }

  // avoid displaying int8_t and uint8_t as a character
  void visit(std::int8_t v)    { _str << int(v) << ' '; }
  void visit(std::uint8_t v)   { _str << unsigned(v) << ' '; }

  void visit(mserialize::Visitor::SequenceBegin sb) { _str << "SB(" << sb.size << ',' << sb.tag << ")[ "; }
  void visit(mserialize::Visitor::SequenceEnd)      { _str << "] "; }

  void visit(mserialize::Visitor::String str)       { _str << "Str(" << str.data << ") "; }

  void visit(mserialize::Visitor::TupleBegin tb)    { _str << "TB(" << tb.tag << ")( "; }
  void visit(mserialize::Visitor::TupleEnd)         { _str << ") "; }

  void visit(mserialize::Visitor::VariantBegin vb)  { _str << "VB(" << vb.discriminator << ',' << vb.tag << ")< "; }
  void visit(mserialize::Visitor::VariantEnd)       { _str << "> "; }
  void visit(mserialize::Visitor::Null)             { _str << "{null} "; }

  void visit(mserialize::Visitor::Enum e)
  {
    _str << "E(" << e.name << "::" << e.enumerator << ',' << e.tag << ",0x" << e.value << ") ";
  }

  void visit(mserialize::Visitor::StructBegin sb)   { _str << "StB(" << sb.name << ',' << sb.tag << ") { "; }
  void visit(mserialize::Visitor::StructEnd)        { _str << "} "; }

  void visit(mserialize::Visitor::FieldBegin fb)    { _str << fb.name << '(' << fb.tag << "): "; }
  void visit(mserialize::Visitor::FieldEnd)         { _str << ", "; }

  std::string value() const { return _str.str(); }
};

template <typename Visitor, typename T, typename IS = InputStream>
typename Visitor::value_type
serialize_and_visit(const T& in)
{
  std::stringstream stream;
  stream.exceptions(std::ios_base::failbit);

  // serialize
  OutputStream ostream{stream};
  mserialize::serialize(in, ostream);

  // visit
  Visitor visitor;
  IS istream{stream};
  const auto tag = mserialize::tag<T>();
  mserialize::visit(tag, visitor, istream);

  return visitor.value();
}

enum class OpaqueEnum : std::int32_t
{
  Unknown = 64
};

struct Empty {};

struct Element { std::string name; int number; };

struct Tree { int value; Tree* left; Tree* right; };

} // namespace

MSERIALIZE_MAKE_ENUM_TAG(OpaqueEnum)
MSERIALIZE_MAKE_ENUM_TAG(test::LargeEnumClass, Golf, Hotel, India, Juliet, Kilo)
MSERIALIZE_MAKE_ENUM_TAG(test::UnsignedLargeEnumClass, Lima, Mike, November, Oscar)

MSERIALIZE_MAKE_STRUCT_SERIALIZABLE(Empty)
MSERIALIZE_MAKE_STRUCT_SERIALIZABLE(Element, name, number)
MSERIALIZE_MAKE_STRUCT_SERIALIZABLE(Tree, value, left, right)

MSERIALIZE_MAKE_STRUCT_TAG(Empty)
MSERIALIZE_MAKE_STRUCT_TAG(Element, name, number)

namespace mserialize {

template <>
struct CustomTag<Tree>
{
  static constexpr auto tag_string()
  {
    return make_cx_string("{Tree`value'i`left'<0{Tree}>`right'<0{Tree}>}");
  }
};

} // namespace mserialize

BOOST_AUTO_TEST_SUITE(MserializeVisit)

BOOST_AUTO_TEST_CASE_TEMPLATE(arithmetic, T, arithmetic_types)
{
  // min
  {
    const T in = std::numeric_limits<T>::min();
    const T out = serialize_and_visit<StoreT<T>>(in);
    BOOST_TEST(in == out);
  }

  // max
  {
    const T in = std::numeric_limits<T>::max();
    const T out = serialize_and_visit<StoreT<T>>(in);
    BOOST_TEST(in == out);
  }
}

BOOST_AUTO_TEST_CASE(empty_vector_of_int)
{
  const std::vector<int> in;
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "SB(0,i)[ ] ");
}

BOOST_AUTO_TEST_CASE(vector_of_int)
{
  const std::vector<int> in{1,2,3,4,5,6};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "SB(6,i)[ 1 2 3 4 5 6 ] ");
}

BOOST_AUTO_TEST_CASE(vector_of_vector_of_int)
{
  const std::vector<std::vector<int>> in{
    {1,2}, {9,8,7}, {3,4}
  };
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "SB(3,[i)[ SB(2,i)[ 1 2 ] SB(3,i)[ 9 8 7 ] SB(2,i)[ 3 4 ] ] ");
}

BOOST_AUTO_TEST_CASE(vector_of_char)
{
  const std::vector<char> in{'f','o','o','b','a','r'};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "SB(6,c)[ f o o b a r ] ");
}

BOOST_AUTO_TEST_CASE(vector_of_char_view_stream)
{
  const std::vector<char> in{'f','o','o','b','a','r'};
  const std::string out = serialize_and_visit<ToString, std::vector<char>, ViewStream>(in);
  BOOST_TEST(out == "Str(foobar) ");
}

BOOST_AUTO_TEST_CASE(string)
{
  const std::string in = "barbaz";
  const std::string out = serialize_and_visit<ToString, std::string, ViewStream>(in);
  BOOST_TEST(out == "Str(barbaz) ");
}

BOOST_AUTO_TEST_CASE(empty_tuple)
{
  const std::tuple<> in;
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "TB()( ) ");
}

BOOST_AUTO_TEST_CASE(tuple_of_int_bool_char_vector_of_int)
{
  const std::tuple<int, bool, char, std::vector<int>> in{123, true, 'A', {4,5,6}};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "TB(iyc[i)( 123 true A SB(3,i)[ 4 5 6 ] ) ");
}

BOOST_AUTO_TEST_CASE(tuple_of_int8_uint8)
{
  // make sure u/int8_t can be displayed as numbers, if the visitor wants to
  const std::tuple<std::int8_t, std::uint8_t> in{41, 42};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "TB(bB)( 41 42 ) ");
}

BOOST_AUTO_TEST_CASE(vector_of_tuple_of_int_bool)
{
  using Tuple = std::tuple<int, bool>;
  const std::vector<Tuple> in{Tuple{123, true}, Tuple{456, false}, Tuple{789, true}};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "SB(3,(iy))[ TB(iy)( 123 true ) TB(iy)( 456 false ) TB(iy)( 789 true ) ] ");
}

BOOST_AUTO_TEST_CASE(null_pointer)
{
  const int* in = nullptr;
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "VB(0,0)< {null} > ");
}

BOOST_AUTO_TEST_CASE(pointer_to_int)
{
  const int value = 123;
  const int* in = &value;
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "VB(1,i)< 123 > ");
}

BOOST_AUTO_TEST_CASE(not_adapted_enum)
{
  const OpaqueEnum in{OpaqueEnum::Unknown};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "E(OpaqueEnum::,i,0x40) ");
}

BOOST_AUTO_TEST_CASE(enum_int64)
{
  // int min value
  {
    const test::LargeEnumClass in{test::LargeEnumClass::Golf};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::LargeEnumClass::Golf,l,0x-8000000000000000) ");
  }

  // negative value
  {
    const test::LargeEnumClass in{test::LargeEnumClass::Hotel};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::LargeEnumClass::Hotel,l,0x-400) ");
  }

  // zero value
  {
    const test::LargeEnumClass in{test::LargeEnumClass::India};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::LargeEnumClass::India,l,0x0) ");
  }

  // positive value
  {
    const test::LargeEnumClass in{test::LargeEnumClass::Juliet};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::LargeEnumClass::Juliet,l,0x800) ");
  }

  // int max value
  {
    const test::LargeEnumClass in{test::LargeEnumClass::Kilo};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::LargeEnumClass::Kilo,l,0x7FFFFFFFFFFFFFFF) ");
  }
}

BOOST_AUTO_TEST_CASE(enum_uint64)
{
  // zero value
  {
    const test::UnsignedLargeEnumClass in{test::UnsignedLargeEnumClass::Lima};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::UnsignedLargeEnumClass::Lima,L,0x0) ");
  }

  // positive value 1 (hex repr. is prefix of positive value 2)
  {
    const test::UnsignedLargeEnumClass in{test::UnsignedLargeEnumClass::Mike};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::UnsignedLargeEnumClass::Mike,L,0x400) ");
  }

  // positive value 2
  {
    const test::UnsignedLargeEnumClass in{test::UnsignedLargeEnumClass::November};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::UnsignedLargeEnumClass::November,L,0x4000) ");
  }

  // int max value
  {
    const test::UnsignedLargeEnumClass in{test::UnsignedLargeEnumClass::Oscar};
    const std::string out = serialize_and_visit<ToString>(in);
    BOOST_TEST(out == "E(test::UnsignedLargeEnumClass::Oscar,L,0xFFFFFFFFFFFFFFFF) ");
  }
}

BOOST_AUTO_TEST_CASE(tuple_of_enum)
{
  const std::tuple<test::LargeEnumClass, test::UnsignedLargeEnumClass> in{
    test::LargeEnumClass::Golf, test::UnsignedLargeEnumClass::Oscar
  };
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "TB("
    "/l`test::LargeEnumClass'-8000000000000000`Golf'-400`Hotel'0`India'800`Juliet'7FFFFFFFFFFFFFFF`Kilo'\\"
    "/L`test::UnsignedLargeEnumClass'0`Lima'400`Mike'4000`November'FFFFFFFFFFFFFFFF`Oscar'\\)( "
    "E(test::LargeEnumClass::Golf,l,0x-8000000000000000) "
    "E(test::UnsignedLargeEnumClass::Oscar,L,0xFFFFFFFFFFFFFFFF) ) "
  );
}

BOOST_AUTO_TEST_CASE(empty_struct)
{
  const Empty in;
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "StB(Empty,) { } ");
}

BOOST_AUTO_TEST_CASE(regular_struct)
{
  const Element in{"Fe", 26};
  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out == "StB(Element,`name'[c`number'i) { name([c): SB(2,c)[ F e ] , number(i): 26 , } ");
}

BOOST_AUTO_TEST_CASE(recursive_struct)
{
  Tree* n = nullptr;

  Tree a{3, n, n}; Tree b{4, n, n};    Tree c{6, n, n}; Tree d{7, n, n};
  //        \             /                     \           /
          Tree e{2, &a, &b};                Tree f{5, &c, &d};
  //                    \                       /
                         const Tree g{1, &e, &f};

  const std::string out = serialize_and_visit<ToString>(g);

  auto leaf = [](int v)
  {
    return "StB(Tree,`value'i`left'<0{Tree}>`right'<0{Tree}>) "
           "{ value(i): " + std::to_string(v) +
           " , left(<0{Tree}>): VB(0,0)< {null} >"
           " , right(<0{Tree}>): VB(0,0)< {null} > , } ";
  };

  auto node = [](int v, const std::string& l, const std::string& r)
  {
    return "StB(Tree,`value'i`left'<0{Tree}>`right'<0{Tree}>) "
           "{ value(i): " + std::to_string(v) +
           " , left(<0{Tree}>): VB(1,{Tree})< " + l + ">"
           " , right(<0{Tree}>): VB(1,{Tree})< " + r + "> , } ";
  };

  const std::string sa = leaf(3);
  const std::string sb = leaf(4);
  const std::string sc = leaf(6);
  const std::string sd = leaf(7);

  const std::string se = node(2, sa, sb);
  const std::string sf = node(5, sc, sd);

  const std::string sg = node(1, se, sf);

  BOOST_TEST(out == sg);
}

BOOST_AUTO_TEST_CASE(tuple_of_recursive_struct)
{
  Tree child{3, nullptr, nullptr};
  const std::tuple<Tree, int, Tree> in{
    Tree{1, nullptr, nullptr},
    123,
    Tree{2, nullptr, &child}
  };

  const std::string out = serialize_and_visit<ToString>(in);
  BOOST_TEST(out ==
   "TB({Tree`value'i`left'<0{Tree}>`right'<0{Tree}>}i{Tree`value'i`left'<0{Tree}>`right'<0{Tree}>})( "
   "StB(Tree,`value'i`left'<0{Tree}>`right'<0{Tree}>) { "
   "value(i): 1 , left(<0{Tree}>): VB(0,0)< {null} > , right(<0{Tree}>): VB(0,0)< {null} > , } "
   "123 "
   "StB(Tree,`value'i`left'<0{Tree}>`right'<0{Tree}>) { "
   "value(i): 2 , left(<0{Tree}>): VB(0,0)< {null} > , right(<0{Tree}>): "
   "VB(1,{Tree})< StB(Tree,`value'i`left'<0{Tree}>`right'<0{Tree}>) { value(i): 3 , left(<0{Tree}>): VB(0,0)< {null} > , right(<0{Tree}>): VB(0,0)< {null} > , } > , "
   "} ) "
  );
}

BOOST_AUTO_TEST_SUITE_END()
