#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace orbit {

enum class ErrorCode : std::uint16_t {
  Format = 1,
  Integrity,
  Unsupported,
  QuerySyntax,
  QueryType,
  Conflict,
  NotFound,
  Temporal,
  Index,
  ResourceLimit,
  Backpressure,
  Cancelled,
  Io,
  InternalInvariant,
  Usage,
};

struct SourceRange {
  std::size_t begin{0};
  std::size_t end{0};
};

struct Error {
  ErrorCode code{ErrorCode::InternalInvariant};
  std::string message;
  std::optional<std::string> component;
  std::optional<std::uint64_t> id;
  std::optional<SourceRange> range;

  [[nodiscard]] std::string describe() const;
};

[[nodiscard]] const char* to_string(ErrorCode code) noexcept;

template <class T>
class Result {
 public:
  Result(T value) : state_(std::move(value)) {}
  Result(Error error) : state_(std::move(error)) {}

  [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(state_); }
  [[nodiscard]] explicit operator bool() const noexcept { return ok(); }

  [[nodiscard]] const T& value() const& { return std::get<T>(state_); }
  [[nodiscard]] T& value() & { return std::get<T>(state_); }
  [[nodiscard]] T&& value() && { return std::move(std::get<T>(state_)); }

  [[nodiscard]] const Error& error() const& { return std::get<Error>(state_); }
  [[nodiscard]] Error& error() & { return std::get<Error>(state_); }

 private:
  std::variant<T, Error> state_;
};

template <>
class Result<void> {
 public:
  Result() = default;
  Result(Error error) : error_(std::move(error)) {}

  [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
  [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
  [[nodiscard]] const Error& error() const& { return *error_; }

 private:
  std::optional<Error> error_;
};

[[nodiscard]] Error make_error(ErrorCode code, std::string message,
                               std::string component = {});

}  // namespace orbit
