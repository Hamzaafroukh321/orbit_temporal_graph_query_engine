#include "orbit/error.hpp"

#include <sstream>

namespace orbit {

const char* to_string(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::Format:
      return "Format";
    case ErrorCode::Integrity:
      return "Integrity";
    case ErrorCode::Unsupported:
      return "Unsupported";
    case ErrorCode::QuerySyntax:
      return "QuerySyntax";
    case ErrorCode::QueryType:
      return "QueryType";
    case ErrorCode::Conflict:
      return "Conflict";
    case ErrorCode::NotFound:
      return "NotFound";
    case ErrorCode::Temporal:
      return "Temporal";
    case ErrorCode::Index:
      return "Index";
    case ErrorCode::ResourceLimit:
      return "ResourceLimit";
    case ErrorCode::Backpressure:
      return "Backpressure";
    case ErrorCode::Cancelled:
      return "Cancelled";
    case ErrorCode::Io:
      return "Io";
    case ErrorCode::InternalInvariant:
      return "InternalInvariant";
    case ErrorCode::Usage:
      return "Usage";
  }
  return "InternalInvariant";
}

std::string Error::describe() const {
  std::ostringstream out;
  out << to_string(code) << ": " << message;
  if (component && !component->empty()) {
    out << " [" << *component << "]";
  }
  if (id) {
    out << " id=" << *id;
  }
  if (range) {
    out << " range=" << range->begin << ".." << range->end;
  }
  return out.str();
}

Error make_error(ErrorCode code, std::string message, std::string component) {
  Error error;
  error.code = code;
  error.message = std::move(message);
  if (!component.empty()) {
    error.component = std::move(component);
  }
  return error;
}

}  // namespace orbit
