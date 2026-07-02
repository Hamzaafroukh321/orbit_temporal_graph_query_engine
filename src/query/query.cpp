#include "orbit/query.hpp"

#include "orbit/store.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <numeric>
#include <set>
#include <sstream>

namespace orbit {
namespace {

enum class Mode {
  Scan,
  Step,
  Path,
};

enum class YieldKind {
  NodeId,
  EdgeId,
  Path,
};

enum class Direction {
  Out,
  In,
};

enum class PredicateOp {
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
};

struct Predicate {
  std::string key;
  PredicateOp op{PredicateOp::Equal};
  PropertyValue value;
};

struct QueryAst {
  std::string source_label;
  std::optional<Predicate> where;
  Mode mode{Mode::Scan};
  Direction direction{Direction::Out};
  std::string edge_type;
  std::size_t path_hops{1};
  std::optional<std::string> cost_property;
  YieldKind yield{YieldKind::NodeId};
};

struct Token {
  std::string text;
  SourceRange range;
};

bool ieq(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (std::toupper(static_cast<unsigned char>(lhs[i])) !=
        std::toupper(static_cast<unsigned char>(rhs[i]))) {
      return false;
    }
  }
  return true;
}

Result<std::vector<Token>> tokenize(std::string_view query, Limits limits) {
  if (query.size() > limits.max_query_bytes) {
    return make_error(ErrorCode::ResourceLimit, "query exceeds configured limit", "oqs");
  }
  std::vector<Token> tokens;
  std::size_t pos = 0;
  while (pos < query.size()) {
    while (pos < query.size() &&
           std::isspace(static_cast<unsigned char>(query[pos])) != 0) {
      ++pos;
    }
    if (pos >= query.size()) {
      break;
    }
    const auto begin = pos;
    if (query[pos] == '"' || query[pos] == '\'') {
      const char quote = query[pos++];
      std::string text;
      while (pos < query.size() && query[pos] != quote) {
        if (query[pos] == '\\' && pos + 1U < query.size()) {
          ++pos;
        }
        text.push_back(query[pos++]);
      }
      if (pos >= query.size()) {
        Error error = make_error(ErrorCode::QuerySyntax, "unterminated string literal", "oqs");
        error.range = SourceRange{begin, query.size()};
        return error;
      }
      ++pos;
      tokens.push_back(Token{text, SourceRange{begin, pos}});
      continue;
    }
    if (query[pos] == '=' || query[pos] == '!' || query[pos] == '<' || query[pos] == '>') {
      std::string op(1, query[pos++]);
      if (pos < query.size() && query[pos] == '=' && op[0] != '=') {
        op.push_back(query[pos++]);
      }
      tokens.push_back(Token{std::move(op), SourceRange{begin, pos}});
      continue;
    }
    if (query[pos] == ',' || query[pos] == '.') {
      tokens.push_back(Token{std::string(1, query[pos]), SourceRange{pos, pos + 1U}});
      ++pos;
      continue;
    }
    while (pos < query.size() &&
           std::isspace(static_cast<unsigned char>(query[pos])) == 0 && query[pos] != '=' &&
           query[pos] != '!' && query[pos] != '<' && query[pos] != '>' && query[pos] != ',') {
      if (query[pos] == '.') {
        const bool decimal_dot = pos > begin && pos + 1U < query.size() &&
                                 std::isdigit(static_cast<unsigned char>(query[pos - 1U])) != 0 &&
                                 std::isdigit(static_cast<unsigned char>(query[pos + 1U])) != 0;
        if (!decimal_dot) {
          break;
        }
      }
      ++pos;
    }
    tokens.push_back(Token{std::string(query.substr(begin, pos - begin)), SourceRange{begin, pos}});
  }
  return tokens;
}

class Parser {
 public:
  Parser(std::vector<Token> tokens, Limits limits) : tokens_(std::move(tokens)), limits_(limits) {}

  Result<QueryAst> parse() {
    QueryAst ast;
    if (match("AT")) {
      if (!match("COMMIT")) {
        return syntax("expected COMMIT after AT");
      }
      if (eof()) {
        return syntax("expected commit selector");
      }
      ++pos_;
      if (!match("TIME")) {
        return syntax("expected TIME after commit selector");
      }
      if (eof()) {
        return syntax("expected valid time selector");
      }
      ++pos_;
    }
    if (!match("FROM")) {
      return syntax("expected FROM");
    }
    if (eof()) {
      return syntax("expected source label");
    }
    ast.source_label = tokens_[pos_++].text;

    if (match("WHERE")) {
      if (eof()) {
        return syntax("expected property name in WHERE");
      }
      const auto key = tokens_[pos_++].text;
      if (eof()) {
        return syntax("expected operator in WHERE predicate");
      }
      auto op = parse_predicate_op(tokens_[pos_++].text);
      if (!op) {
        return op.error();
      }
      if (eof()) {
        return syntax("expected literal in WHERE predicate");
      }
      ast.where = Predicate{key, op.value(), parse_literal(tokens_[pos_++].text)};
    }

    if (match("STEP")) {
      ast.mode = Mode::Step;
      auto direction = parse_direction("STEP");
      if (!direction) {
        return direction.error();
      }
      ast.direction = direction.value();
      if (eof()) {
        return syntax("expected edge type after STEP direction");
      }
      ast.edge_type = tokens_[pos_++].text;
    } else if (match("PATH")) {
      ast.mode = Mode::Path;
      auto direction = parse_direction("PATH");
      if (!direction) {
        return direction.error();
      }
      ast.direction = direction.value();
      if (eof()) {
        return syntax("expected edge type after PATH direction");
      }
      ast.edge_type = tokens_[pos_++].text;
      if (match("HOPS")) {
        if (eof()) {
          return syntax("expected hop bound");
        }
        auto hops = parse_u64(tokens_[pos_++].text);
        if (!hops) {
          return hops.error();
        }
        if (hops.value() == 0 || hops.value() > limits_.max_path_hops) {
          return make_error(ErrorCode::ResourceLimit, "path hop bound exceeds configured limit",
                            "oqs");
        }
        ast.path_hops = static_cast<std::size_t>(hops.value());
      }
      if (match("COST")) {
        if (eof()) {
          return syntax("expected edge cost property");
        }
        ast.cost_property = tokens_[pos_++].text;
      }
    }

    if (!match("YIELD")) {
      return syntax("expected YIELD");
    }
    if (match("node")) {
      if (!match(".") || !match("id")) {
        return syntax("expected node.id");
      }
      ast.yield = YieldKind::NodeId;
    } else if (match("edge")) {
      if (!match(".") || !match("id")) {
        return syntax("expected edge.id");
      }
      ast.yield = YieldKind::EdgeId;
    } else if (match("path")) {
      ast.yield = YieldKind::Path;
    } else {
      return syntax("expected node.id, edge.id, or path");
    }
    if (!eof()) {
      return syntax("unexpected tokens after query");
    }
    return ast;
  }

 private:
  bool eof() const noexcept { return pos_ >= tokens_.size(); }

  bool match(std::string_view text) {
    if (!eof() && ieq(tokens_[pos_].text, text)) {
      ++pos_;
      return true;
    }
    return false;
  }

  Result<QueryAst> syntax(std::string message) const {
    Error error = make_error(ErrorCode::QuerySyntax, std::move(message), "oqs");
    if (!eof()) {
      error.range = tokens_[pos_].range;
    }
    return error;
  }

  Result<std::uint64_t> parse_u64(const std::string& text) const {
    std::uint64_t value = 0;
    for (char ch : text) {
      if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
        Error error = make_error(ErrorCode::QuerySyntax, "expected unsigned integer", "oqs");
        return error;
      }
      const auto digit = static_cast<std::uint64_t>(ch - '0');
      auto mul = checked_mul(value, 10);
      if (!mul) {
        return mul.error();
      }
      auto add = checked_add(mul.value(), digit);
      if (!add) {
        return add.error();
      }
      value = add.value();
    }
    return value;
  }

  Result<Direction> parse_direction(std::string_view clause) {
    if (match("OUT")) {
      return Direction::Out;
    }
    if (match("IN")) {
      return Direction::In;
    }
    Error error = make_error(ErrorCode::QuerySyntax,
                             "expected IN or OUT after " + std::string(clause), "oqs");
    if (!eof()) {
      error.range = tokens_[pos_].range;
    }
    return error;
  }

  Result<PredicateOp> parse_predicate_op(const std::string& text) const {
    if (text == "=") {
      return PredicateOp::Equal;
    }
    if (text == "!=") {
      return PredicateOp::NotEqual;
    }
    if (text == "<") {
      return PredicateOp::Less;
    }
    if (text == "<=") {
      return PredicateOp::LessEqual;
    }
    if (text == ">") {
      return PredicateOp::Greater;
    }
    if (text == ">=") {
      return PredicateOp::GreaterEqual;
    }
    Error error = make_error(ErrorCode::QuerySyntax, "expected comparison operator", "oqs");
    if (pos_ > 0) {
      error.range = tokens_[pos_ - 1U].range;
    }
    return error;
  }

  PropertyValue parse_literal(const std::string& text) const {
    if (ieq(text, "true")) {
      return true;
    }
    if (ieq(text, "false")) {
      return false;
    }
    bool numeric = !text.empty();
    bool decimal = false;
    std::size_t start = 0;
    if (!text.empty() && text[0] == '-') {
      start = 1;
    }
    for (std::size_t i = start; i < text.size(); ++i) {
      if (text[i] == '.' && !decimal) {
        decimal = true;
        continue;
      }
      numeric = numeric && std::isdigit(static_cast<unsigned char>(text[i])) != 0;
    }
    if (numeric && start < text.size()) {
      if (decimal) {
        return std::stod(text);
      }
      return static_cast<std::int64_t>(std::stoll(text));
    }
    return text;
  }

  std::vector<Token> tokens_;
  Limits limits_;
  std::size_t pos_{0};
};

bool is_numeric(const PropertyValue& value) {
  return std::holds_alternative<std::int64_t>(value) || std::holds_alternative<double>(value);
}

double numeric_value(const PropertyValue& value) {
  if (std::holds_alternative<std::int64_t>(value)) {
    return static_cast<double>(std::get<std::int64_t>(value));
  }
  return std::get<double>(value);
}

std::string predicate_op_name(PredicateOp op) {
  switch (op) {
    case PredicateOp::Equal:
      return "=";
    case PredicateOp::NotEqual:
      return "!=";
    case PredicateOp::Less:
      return "<";
    case PredicateOp::LessEqual:
      return "<=";
    case PredicateOp::Greater:
      return ">";
    case PredicateOp::GreaterEqual:
      return ">=";
  }
  return "?";
}

bool apply_comparison(int comparison, PredicateOp op) {
  switch (op) {
    case PredicateOp::Equal:
      return comparison == 0;
    case PredicateOp::NotEqual:
      return comparison != 0;
    case PredicateOp::Less:
      return comparison < 0;
    case PredicateOp::LessEqual:
      return comparison <= 0;
    case PredicateOp::Greater:
      return comparison > 0;
    case PredicateOp::GreaterEqual:
      return comparison >= 0;
  }
  return false;
}

Result<bool> compare_property(const PropertyValue& lhs, PredicateOp op, const PropertyValue& rhs) {
  if (is_numeric(lhs) && is_numeric(rhs)) {
    const double left = numeric_value(lhs);
    const double right = numeric_value(rhs);
    if (!std::isfinite(left) || !std::isfinite(right)) {
      return make_error(ErrorCode::QueryType, "numeric predicate value must be finite", "query");
    }
    const int comparison = left < right ? -1 : (left > right ? 1 : 0);
    return apply_comparison(comparison, op);
  }
  if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs)) {
    const auto& left = std::get<std::string>(lhs);
    const auto& right = std::get<std::string>(rhs);
    const int comparison = left < right ? -1 : (left > right ? 1 : 0);
    return apply_comparison(comparison, op);
  }
  if (std::holds_alternative<bool>(lhs) && std::holds_alternative<bool>(rhs)) {
    if (op != PredicateOp::Equal && op != PredicateOp::NotEqual) {
      return make_error(ErrorCode::QueryType, "boolean predicates only support = and !=", "query");
    }
    return apply_comparison(std::get<bool>(lhs) == std::get<bool>(rhs) ? 0 : 1, op);
  }
  if (op == PredicateOp::Equal) {
    return false;
  }
  if (op == PredicateOp::NotEqual) {
    return true;
  }
  return make_error(ErrorCode::QueryType, "predicate operands have incompatible types", "query");
}

Result<bool> property_matches(const NodeVersionView& node, const std::optional<Predicate>& predicate) {
  if (!predicate) {
    return true;
  }
  const auto found = node.properties.find(predicate->key);
  if (found == node.properties.end()) {
    return false;
  }
  return compare_property(found->second, predicate->op, predicate->value);
}

bool label_matches(const NodeVersionView& node, std::string_view label) {
  return node.label == label;
}

std::optional<NodeVersionView> find_node(const GraphSnapshot& snapshot, NodeId id) {
  return snapshot.node(id);
}

std::string path_string(const std::vector<NodeId>& nodes) {
  std::ostringstream out;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (i != 0) {
      out << "->";
    }
    out << nodes[i].value;
  }
  return out.str();
}

Result<double> edge_cost(const EdgeVersionView& edge, const std::string& property) {
  const auto found = edge.properties.find(property);
  if (found == edge.properties.end()) {
    return make_error(ErrorCode::QueryType, "edge is missing path cost property", "query");
  }
  return std::visit(
      [](const auto& value) -> Result<double> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::int64_t>) {
          if (value < 0) {
            return make_error(ErrorCode::QueryType, "path cost must be nonnegative", "query");
          }
          return static_cast<double>(value);
        } else if constexpr (std::is_same_v<T, double>) {
          if (!std::isfinite(value) || value < 0.0) {
            return make_error(ErrorCode::QueryType, "path cost must be finite and nonnegative",
                              "query");
          }
          return value;
        } else {
          return make_error(ErrorCode::QueryType, "path cost property must be numeric", "query");
        }
      },
      found->second);
}

std::string direction_name(Direction direction) {
  return direction == Direction::Out ? "out" : "in";
}

}  // namespace

struct PreparedQuery::Impl {
  QueryAst ast;
  std::string fingerprint;
};

CancelToken::CancelToken() : cancelled_(std::make_shared<bool>(false)) {}

void CancelToken::cancel() noexcept {
  *cancelled_ = true;
}

bool CancelToken::cancelled() const noexcept {
  return *cancelled_;
}

ResultCursor::ResultCursor(std::vector<QueryRow> rows) : rows_(std::move(rows)) {}

ResultCursor::ResultCursor(std::vector<QueryRow> rows, std::vector<std::string> continuation_keys)
    : rows_(std::move(rows)), continuation_keys_(std::move(continuation_keys)) {
  if (continuation_keys_.size() != rows_.size()) {
    continuation_keys_.clear();
  }
}

Result<std::optional<ResultBatch>> ResultCursor::next(std::size_t row_budget) {
  if (cancelled_) {
    return make_error(ErrorCode::Cancelled, "cursor is cancelled", "cursor");
  }
  if (row_budget == 0) {
    return make_error(ErrorCode::ResourceLimit, "row budget must be positive", "cursor");
  }
  if (offset_ >= rows_.size()) {
    return std::optional<ResultBatch>{std::nullopt};
  }
  const auto take = std::min(row_budget, rows_.size() - offset_);
  ResultBatch batch;
  batch.rows.insert(batch.rows.end(), rows_.begin() + static_cast<std::ptrdiff_t>(offset_),
                    rows_.begin() + static_cast<std::ptrdiff_t>(offset_ + take));
  offset_ += take;
  batch.complete = offset_ >= rows_.size();
  if (!continuation_keys_.empty() && offset_ > 0) {
    batch.continuation_key = continuation_keys_[offset_ - 1U];
  }
  return std::optional<ResultBatch>{std::move(batch)};
}

void ResultCursor::cancel() noexcept {
  cancelled_ = true;
}

bool ResultCursor::done() const noexcept {
  return offset_ >= rows_.size();
}

PreparedQuery::PreparedQuery() = default;
PreparedQuery::PreparedQuery(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}

Result<ResultCursor> PreparedQuery::execute(const GraphSnapshot& snapshot, QueryLimits limits) const {
  return execute(snapshot, limits, CancelToken{});
}

Result<ResultCursor> PreparedQuery::execute(const GraphSnapshot& snapshot, QueryLimits limits,
                                            const CancelToken& cancel) const {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized prepared query", "query");
  }
  std::vector<QueryRow> rows;
  std::vector<std::string> continuation_keys;
  std::vector<double> row_costs;
  std::size_t work_units = 0;
  auto poll = [&]() -> Result<void> {
    if (cancel.cancelled()) {
      return make_error(ErrorCode::Cancelled, "query execution cancelled", "query");
    }
    ++work_units;
    if (work_units > limits.work_limit) {
      return make_error(ErrorCode::ResourceLimit, "query work limit exceeded", "query");
    }
    return {};
  };
  auto emit_node = [&](NodeId id, std::string key, double cost) -> Result<void> {
    auto polled = poll();
    if (!polled) {
      return polled;
    }
    if (rows.size() >= limits.row_limit) {
      return make_error(ErrorCode::ResourceLimit, "query row limit exceeded", "query");
    }
    rows.push_back(QueryRow{{static_cast<std::int64_t>(id.value)}});
    continuation_keys.push_back(std::move(key));
    row_costs.push_back(cost);
    return {};
  };
  auto emit_edge = [&](EdgeId id, std::string key, double cost) -> Result<void> {
    auto polled = poll();
    if (!polled) {
      return polled;
    }
    if (rows.size() >= limits.row_limit) {
      return make_error(ErrorCode::ResourceLimit, "query row limit exceeded", "query");
    }
    rows.push_back(QueryRow{{static_cast<std::int64_t>(id.value)}});
    continuation_keys.push_back(std::move(key));
    row_costs.push_back(cost);
    return {};
  };
  auto emit_path = [&](const std::vector<NodeId>& path, std::string key, double cost) -> Result<void> {
    auto polled = poll();
    if (!polled) {
      return polled;
    }
    if (rows.size() >= limits.row_limit) {
      return make_error(ErrorCode::ResourceLimit, "query row limit exceeded", "query");
    }
    rows.push_back(QueryRow{{path_string(path)}});
    continuation_keys.push_back(std::move(key));
    row_costs.push_back(cost);
    return {};
  };

  std::vector<NodeVersionView> seeds;
  if (impl_->ast.where && impl_->ast.where->op == PredicateOp::Equal) {
    seeds = snapshot.nodes_with_property(impl_->ast.where->key, impl_->ast.where->value);
    seeds.erase(std::remove_if(seeds.begin(), seeds.end(),
                               [&](const NodeVersionView& node) {
                                 return !label_matches(node, impl_->ast.source_label);
                               }),
                seeds.end());
  } else {
    seeds = snapshot.nodes_with_label(impl_->ast.source_label);
    if (impl_->ast.where) {
      std::vector<NodeVersionView> filtered;
      filtered.reserve(seeds.size());
      for (const auto& node : seeds) {
        auto matches = property_matches(node, impl_->ast.where);
        if (!matches) {
          return matches.error();
        }
        if (matches.value()) {
          filtered.push_back(node);
        }
      }
      seeds = std::move(filtered);
    }
  }
  std::sort(seeds.begin(), seeds.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });

  if (impl_->ast.mode == Mode::Scan) {
    for (const auto& node : seeds) {
      auto emitted = emit_node(node.id, "scan:" + std::to_string(node.id.value), 0.0);
      if (!emitted) {
        return emitted.error();
      }
    }
    return ResultCursor{std::move(rows), std::move(continuation_keys)};
  }

  if (impl_->ast.mode == Mode::Step) {
    for (const auto& seed : seeds) {
      auto polled = poll();
      if (!polled) {
        return polled.error();
      }
      const auto adjacent = impl_->ast.direction == Direction::Out
                                ? snapshot.out_edges(seed.id, impl_->ast.edge_type)
                                : snapshot.in_edges(seed.id, impl_->ast.edge_type);
      for (const auto& edge : adjacent) {
        polled = poll();
        if (!polled) {
          return polled.error();
        }
        const NodeId adjacent_node = impl_->ast.direction == Direction::Out ? edge.to : edge.from;
        if (!find_node(snapshot, adjacent_node)) {
          continue;
        }
        const std::string key = "adj:" + std::to_string(seed.id.value) + ":" +
                                std::to_string(edge.id.value) + ":" +
                                std::to_string(adjacent_node.value);
        auto emitted = impl_->ast.yield == YieldKind::EdgeId ? emit_edge(edge.id, key, 0.0)
                                                             : emit_node(adjacent_node, key, 0.0);
        if (!emitted) {
          return emitted.error();
        }
      }
    }
    return ResultCursor{std::move(rows), std::move(continuation_keys)};
  }

  if (impl_->ast.path_hops > limits.path_hop_limit) {
    return make_error(ErrorCode::ResourceLimit, "query path hop bound exceeds execution limit",
                      "query");
  }
  const std::size_t hop_bound = impl_->ast.path_hops;
  for (const auto& seed : seeds) {
    struct Frontier {
      NodeId node;
      std::vector<NodeId> path;
      std::size_t hops{0};
      double cost{0.0};
    };
    std::deque<Frontier> queue;
    queue.push_back(Frontier{seed.id, {seed.id}, 0});
    while (!queue.empty()) {
      auto polled = poll();
      if (!polled) {
        return polled.error();
      }
      if (queue.size() > limits.frontier_limit) {
        return make_error(ErrorCode::ResourceLimit, "path frontier limit exceeded", "query");
      }
      auto current = queue.front();
      queue.pop_front();
      if (current.hops >= hop_bound) {
        continue;
      }
      const auto adjacent = impl_->ast.direction == Direction::Out
                                ? snapshot.out_edges(current.node, impl_->ast.edge_type)
                                : snapshot.in_edges(current.node, impl_->ast.edge_type);
      for (const auto& edge : adjacent) {
        polled = poll();
        if (!polled) {
          return polled.error();
        }
        const NodeId adjacent_node = impl_->ast.direction == Direction::Out ? edge.to : edge.from;
        if (std::find(current.path.begin(), current.path.end(), adjacent_node) != current.path.end()) {
          continue;
        }
        double next_cost = current.cost + 1.0;
        if (impl_->ast.cost_property) {
          auto cost = edge_cost(edge, *impl_->ast.cost_property);
          if (!cost) {
            return cost.error();
          }
          next_cost = current.cost + cost.value();
        }
        auto next_path = current.path;
        next_path.push_back(adjacent_node);
        const std::string key = "path:" + path_string(next_path);
        if (impl_->ast.yield == YieldKind::Path) {
          auto emitted = emit_path(next_path, key, next_cost);
          if (!emitted) {
            return emitted.error();
          }
        } else if (impl_->ast.yield == YieldKind::EdgeId) {
          auto emitted = emit_edge(edge.id, key, next_cost);
          if (!emitted) {
            return emitted.error();
          }
        } else {
          auto emitted = emit_node(adjacent_node, key, next_cost);
          if (!emitted) {
            return emitted.error();
          }
        }
        if (queue.size() >= limits.frontier_limit) {
          return make_error(ErrorCode::ResourceLimit, "path frontier limit exceeded", "query");
        }
        queue.push_back(Frontier{adjacent_node, std::move(next_path), current.hops + 1U,
                                 next_cost});
      }
    }
  }
  if (impl_->ast.cost_property) {
    std::vector<std::size_t> order(rows.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(), [&](std::size_t lhs, std::size_t rhs) {
      if (row_costs[lhs] != row_costs[rhs]) {
        return row_costs[lhs] < row_costs[rhs];
      }
      return continuation_keys[lhs] < continuation_keys[rhs];
    });
    std::vector<QueryRow> sorted_rows;
    std::vector<std::string> sorted_keys;
    sorted_rows.reserve(rows.size());
    sorted_keys.reserve(continuation_keys.size());
    for (const auto index : order) {
      sorted_rows.push_back(std::move(rows[index]));
      sorted_keys.push_back(std::move(continuation_keys[index]));
    }
    rows = std::move(sorted_rows);
    continuation_keys = std::move(sorted_keys);
  }
  return ResultCursor{std::move(rows), std::move(continuation_keys)};
}

ExplainPlan PreparedQuery::explain() const {
  if (!impl_) {
    return {};
  }
  ExplainPlan plan;
  plan.fingerprint = impl_->fingerprint;
  if (impl_->ast.where) {
    if (impl_->ast.where->op == PredicateOp::Equal) {
      plan.operators.push_back("property-index-seek(" + impl_->ast.where->key + ")");
      plan.operators.push_back("label-filter(" + impl_->ast.source_label + ")");
    } else {
      plan.operators.push_back("label-index-seek(" + impl_->ast.source_label + ")");
    }
  } else {
    plan.operators.push_back("label-index-seek(" + impl_->ast.source_label + ")");
  }
  if (impl_->ast.where) {
    plan.operators.push_back("property-filter(" + impl_->ast.where->key +
                             predicate_op_name(impl_->ast.where->op) + ")");
  }
  if (impl_->ast.mode == Mode::Step) {
    plan.operators.push_back("adjacency-expand-" + direction_name(impl_->ast.direction) +
                             "(type=" + impl_->ast.edge_type + ")");
  } else if (impl_->ast.mode == Mode::Path) {
    plan.operators.push_back("bounded-bfs-" + direction_name(impl_->ast.direction) +
                             "(type=" + impl_->ast.edge_type + ")");
    if (impl_->ast.cost_property) {
      plan.operators.push_back("cost-order(" + *impl_->ast.cost_property + ")");
    }
  }
  plan.operators.push_back("temporal-filter(commit+valid-time)");
  plan.operators.push_back("project");
  return plan;
}

Result<PreparedQuery> prepare_query(std::string_view query, Limits limits) {
  auto tokens = tokenize(query, limits);
  if (!tokens) {
    return tokens.error();
  }
  Parser parser{tokens.value(), limits};
  auto ast = parser.parse();
  if (!ast) {
    return ast.error();
  }
  auto impl = std::make_shared<PreparedQuery::Impl>();
  impl->ast = ast.value();
  std::ostringstream fingerprint;
  fingerprint << "from=" << impl->ast.source_label << ";mode=" << static_cast<int>(impl->ast.mode)
              << ";dir=" << direction_name(impl->ast.direction) << ";edge=" << impl->ast.edge_type << ";yield="
              << static_cast<int>(impl->ast.yield) << ";hops=" << impl->ast.path_hops;
  if (impl->ast.cost_property) {
    fingerprint << ";cost=" << *impl->ast.cost_property;
  }
  if (impl->ast.where) {
    fingerprint << ";where=" << impl->ast.where->key
                << predicate_op_name(impl->ast.where->op)
                << canonical_value(impl->ast.where->value);
  }
  impl->fingerprint = fingerprint.str();
  return PreparedQuery{impl};
}

}  // namespace orbit
