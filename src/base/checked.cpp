#include "orbit/value.hpp"

#include <limits>

namespace orbit {

Result<std::uint64_t> checked_add(std::uint64_t a, std::uint64_t b) {
  if (a > std::numeric_limits<std::uint64_t>::max() - b) {
    return make_error(ErrorCode::ResourceLimit, "unsigned addition overflow", "checked");
  }
  return a + b;
}

Result<std::uint64_t> checked_mul(std::uint64_t a, std::uint64_t b) {
  if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
    return make_error(ErrorCode::ResourceLimit, "unsigned multiplication overflow", "checked");
  }
  return a * b;
}

}  // namespace orbit
