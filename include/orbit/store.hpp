#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "orbit/query.hpp"
#include "orbit/value.hpp"

namespace orbit {

struct OpenOptions {
  bool create_if_missing{false};
  Limits limits{};
};

struct SnapshotSelector {
  std::optional<CommitSeq> commit;
  std::int64_t valid_time{0};
};

struct NodeVersionView {
  NodeId id;
  std::string label;
  Interval interval;
  PropertyMap properties;
  CommitSeq begin_commit;
};

struct EdgeVersionView {
  EdgeId id;
  NodeId from;
  NodeId to;
  std::string type;
  Interval interval;
  PropertyMap properties;
  CommitSeq begin_commit;
};

class GraphSnapshot {
 public:
  struct Impl;

  GraphSnapshot();
  explicit GraphSnapshot(std::shared_ptr<const Impl> impl);

  [[nodiscard]] CommitSeq commit() const;
  [[nodiscard]] std::int64_t valid_time() const;
  [[nodiscard]] const std::vector<NodeVersionView>& nodes() const;
  [[nodiscard]] const std::vector<EdgeVersionView>& edges() const;
  [[nodiscard]] std::optional<NodeVersionView> node(NodeId id) const;

 private:
  std::shared_ptr<const Impl> impl_;
};

class GraphStore;

class Transaction {
 public:
  struct Impl;

  Transaction();
  explicit Transaction(std::unique_ptr<Impl> impl);
  Transaction(Transaction&&) noexcept;
  Transaction& operator=(Transaction&&) noexcept;
  ~Transaction();

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  [[nodiscard]] Result<void> put_node(NodeId id, std::string label, Interval interval,
                                      PropertyMap properties = {});
  [[nodiscard]] Result<void> put_edge(EdgeId id, NodeId from, NodeId to, std::string type,
                                      Interval interval, PropertyMap properties = {});
  [[nodiscard]] Result<void> delete_node(NodeId id);
  [[nodiscard]] Result<void> delete_edge(EdgeId id);
  [[nodiscard]] Result<CommitSeq> commit();
  void abort() noexcept;

 private:
  std::unique_ptr<Impl> impl_;
};

class GraphStore {
 public:
  struct Impl;

  GraphStore();
  explicit GraphStore(std::shared_ptr<Impl> impl);

  [[nodiscard]] static Result<GraphStore> open(const std::filesystem::path& path,
                                               OpenOptions options = {});
  [[nodiscard]] Result<Transaction> begin();
  [[nodiscard]] Result<GraphSnapshot> snapshot(SnapshotSelector selector = {}) const;
  [[nodiscard]] Result<PreparedQuery> prepare(std::string_view query) const;
  [[nodiscard]] CommitSeq latest_commit() const;
  [[nodiscard]] Result<void> check() const;
  [[nodiscard]] std::string inspect() const;

 private:
  std::shared_ptr<Impl> impl_;
};

}  // namespace orbit
