#include "orbit/value.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace orbit {

bool Interval::contains(std::int64_t time) const noexcept {
  return start <= time && time < end;
}

bool Interval::overlaps(const Interval& other) const noexcept {
  return start < other.end && other.start < end;
}

bool Interval::valid() const noexcept {
  return start < end;
}

Result<void> validate_interval(Interval interval) {
  if (!interval.valid()) {
    return make_error(ErrorCode::Temporal, "interval must be half-open with start < end",
                      "interval");
  }
  return {};
}

std::string canonical_value(const PropertyValue& value) {
  return std::visit(
      [](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
          return item ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          return std::to_string(item);
        } else if constexpr (std::is_same_v<T, double>) {
          if (!std::isfinite(item)) {
            return "nan";
          }
          std::ostringstream out;
          out << std::setprecision(17) << item;
          return out.str();
        } else {
          std::ostringstream out;
          out << '"';
          for (char ch : item) {
            if (ch == '"' || ch == '\\') {
              out << '\\';
            }
            out << ch;
          }
          out << '"';
          return out.str();
        }
      },
      value);
}

std::string canonical_properties(const PropertyMap& properties) {
  std::ostringstream out;
  bool first = true;
  for (const auto& [key, value] : properties) {
    if (!first) {
      out << ",";
    }
    first = false;
    out << key << "=" << canonical_value(value);
  }
  return out.str();
}

}  // namespace orbit
