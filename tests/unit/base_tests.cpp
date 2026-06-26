#include "test_support.hpp"

#include "orbit/value.hpp"

ORBIT_TEST(CheckedAddDetectsOverflow) {
  auto result = orbit::checked_add(UINT64_MAX, 1);
  REQUIRE(!result);
}

ORBIT_TEST(CheckedMulDetectsOverflow) {
  auto result = orbit::checked_mul(UINT64_MAX, 2);
  REQUIRE(!result);
}

ORBIT_TEST(CheckedArithmeticAllowsNormalValues) {
  auto result = orbit::checked_mul(7, 6);
  REQUIRE(result);
  REQUIRE(result.value() == 42);
}

ORBIT_TEST(IntervalIncludesStartExcludesEnd) {
  orbit::Interval interval{10, 20};
  REQUIRE(interval.contains(10));
  REQUIRE(!interval.contains(20));
}

ORBIT_TEST(IntervalRejectsEmptyRange) {
  auto result = orbit::validate_interval(orbit::Interval{5, 5});
  REQUIRE(!result);
}

ORBIT_TEST(IntervalOverlapUsesHalfOpenSemantics) {
  const orbit::Interval first{0, 5};
  const orbit::Interval second{5, 10};
  const orbit::Interval overlapping{0, 6};
  REQUIRE(!first.overlaps(second));
  REQUIRE(overlapping.overlaps(second));
}

ORBIT_TEST(CanonicalPropertiesAreSorted) {
  orbit::PropertyMap props;
  props["z"] = std::int64_t{1};
  props["a"] = std::string{"x"};
  REQUIRE(orbit::canonical_properties(props) == "a=\"x\",z=1");
}

ORBIT_TEST(CanonicalBoolValuesAreStable) {
  REQUIRE(orbit::canonical_value(orbit::PropertyValue{true}) == "true");
}
