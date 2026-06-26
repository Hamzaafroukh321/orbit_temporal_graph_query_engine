#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "orbit/error.hpp"

namespace orbit {

struct NodeId {
  std::uint64_t value{0};
  friend auto operator<=>(const NodeId&, const NodeId&) = default;
};

struct EdgeId {
  std::uint64_t value{0};
  friend auto operator<=>(const EdgeId&, const EdgeId&) = default;
};

struct CommitSeq {
  std::uint64_t value{0};
  friend auto operator<=>(const CommitSeq&, const CommitSeq&) = default;
};

struct Interval {
  std::int64_t start{0};
  std::int64_t end{0};

  [[nodiscard]] bool contains(std::int64_t time) const noexcept;
  [[nodiscard]] bool overlaps(const Interval& other) const noexcept;
  [[nodiscard]] bool valid() const noexcept;
};

using PropertyValue = std::variant<bool, std::int64_t, double, std::string>;
using PropertyMap = std::map<std::string, PropertyValue>;

struct Limits {
  std::size_t max_string_bytes{1 << 20};
  std::size_t max_properties{1024};
  std::size_t max_record_bytes{32U << 20U};
  std::size_t max_query_bytes{1 << 20};
  std::size_t max_path_hops{32};
  std::size_t max_batch_rows{10000};
};

[[nodiscard]] Result<std::uint64_t> checked_add(std::uint64_t a, std::uint64_t b);
[[nodiscard]] Result<std::uint64_t> checked_mul(std::uint64_t a, std::uint64_t b);
[[nodiscard]] Result<void> validate_interval(Interval interval);
[[nodiscard]] std::string canonical_value(const PropertyValue& value);
[[nodiscard]] std::string canonical_properties(const PropertyMap& properties);

}  // namespace orbit
