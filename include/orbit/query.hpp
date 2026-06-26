#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "orbit/error.hpp"
#include "orbit/value.hpp"

namespace orbit {

class GraphSnapshot;

struct QueryLimits {
  std::size_t row_limit{10000};
  std::size_t batch_limit{1024};
  std::size_t path_hop_limit{32};
};

struct QueryRow {
  std::vector<PropertyValue> values;
};

struct ResultBatch {
  std::vector<QueryRow> rows;
  std::optional<std::string> continuation_key;
  bool complete{false};
};

class ResultCursor {
 public:
  ResultCursor() = default;
  explicit ResultCursor(std::vector<QueryRow> rows);
  ResultCursor(std::vector<QueryRow> rows, std::vector<std::string> continuation_keys);

  [[nodiscard]] Result<std::optional<ResultBatch>> next(std::size_t row_budget);
  void cancel() noexcept;
  [[nodiscard]] bool done() const noexcept;

 private:
  std::vector<QueryRow> rows_;
  std::vector<std::string> continuation_keys_;
  std::size_t offset_{0};
  bool cancelled_{false};
};

struct ExplainPlan {
  std::string fingerprint;
  std::vector<std::string> operators;
};

class PreparedQuery {
 public:
  struct Impl;

  PreparedQuery();
  explicit PreparedQuery(std::shared_ptr<const Impl> impl);

  [[nodiscard]] Result<ResultCursor> execute(const GraphSnapshot& snapshot,
                                             QueryLimits limits = {}) const;
  [[nodiscard]] ExplainPlan explain() const;

 private:
  std::shared_ptr<const Impl> impl_;
};

[[nodiscard]] Result<PreparedQuery> prepare_query(std::string_view query,
                                                  Limits limits = {});

}  // namespace orbit
