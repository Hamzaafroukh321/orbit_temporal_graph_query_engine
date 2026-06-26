#include "orbit/store.hpp"

#include "orbit/format.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string_view>

namespace orbit {
namespace {

struct NodeRecord {
  NodeVersionView view;
  bool tombstone{false};
};

struct EdgeRecord {
  EdgeVersionView view;
  bool tombstone{false};
};

template <class Id, class Record>
using VersionMap = std::map<Id, std::vector<Record>>;

bool visible_commit(CommitSeq begin, CommitSeq selector) {
  return begin.value <= selector.value;
}

template <class Id, class Record>
const Record* latest_at(const VersionMap<Id, Record>& versions, Id id, CommitSeq commit) {
  const auto found = versions.find(id);
  if (found == versions.end()) {
    return nullptr;
  }
  const Record* latest = nullptr;
  for (const auto& record : found->second) {
    if (visible_commit(record.view.begin_commit, commit)) {
      latest = &record;
    }
  }
  return latest;
}

Error invalid_id(std::string entity) {
  return make_error(ErrorCode::Usage, std::move(entity) + " id must be nonzero", "store");
}

std::string property_index_key(std::string_view key, const PropertyValue& value) {
  std::string joined(key);
  joined.push_back('\0');
  joined += canonical_value(value);
  return joined;
}

template <class View, class IdFn>
std::vector<View> select_active_at(std::vector<View> candidates, std::int64_t valid_time,
                                   IdFn id_of) {
  struct Entry {
    Interval interval;
    std::uint64_t stable_id{0};
    std::size_t index{0};
  };

  std::vector<Entry> interval_index;
  interval_index.reserve(candidates.size());
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    interval_index.push_back(Entry{candidates[i].interval, id_of(candidates[i]), i});
  }
  std::sort(interval_index.begin(), interval_index.end(), [](const Entry& lhs, const Entry& rhs) {
    if (lhs.interval.start != rhs.interval.start) {
      return lhs.interval.start < rhs.interval.start;
    }
    if (lhs.interval.end != rhs.interval.end) {
      return lhs.interval.end < rhs.interval.end;
    }
    return lhs.stable_id < rhs.stable_id;
  });

  std::vector<View> active;
  for (const auto& entry : interval_index) {
    if (entry.interval.start > valid_time) {
      break;
    }
    if (entry.interval.contains(valid_time)) {
      active.push_back(candidates[entry.index]);
    }
  }
  return active;
}

}  // namespace

struct GraphSnapshot::Impl {
  struct LeaseRegistry {
    mutable std::mutex mutex;
    std::map<std::uint64_t, std::size_t> pins;
    std::set<std::uint64_t> known_generations;
    std::uint64_t latest_generation{1};

    void register_generation(std::uint64_t generation) {
      std::lock_guard<std::mutex> lock(mutex);
      latest_generation = std::max(latest_generation, generation);
      known_generations.insert(generation);
    }

    void pin(std::uint64_t generation) {
      std::lock_guard<std::mutex> lock(mutex);
      latest_generation = std::max(latest_generation, generation);
      known_generations.insert(generation);
      ++pins[generation];
    }

    void unpin(std::uint64_t generation) noexcept {
      std::lock_guard<std::mutex> lock(mutex);
      const auto found = pins.find(generation);
      if (found == pins.end()) {
        return;
      }
      if (found->second <= 1U) {
        pins.erase(found);
      } else {
        --found->second;
      }
    }

    CacheStats stats() const {
      std::lock_guard<std::mutex> lock(mutex);
      CacheStats result;
      result.known_generations = known_generations.size();
      result.pinned_generations = pins.size();
      for (const auto& [generation, count] : pins) {
        (void)generation;
        result.total_pins += count;
      }
      return result;
    }

    std::size_t evict_unpinned() {
      std::lock_guard<std::mutex> lock(mutex);
      std::size_t evicted = 0;
      for (auto it = known_generations.begin(); it != known_generations.end();) {
        if (*it < latest_generation && !pins.contains(*it)) {
          it = known_generations.erase(it);
          ++evicted;
        } else {
          ++it;
        }
      }
      return evicted;
    }
  };

  CommitSeq commit;
  std::int64_t valid_time{0};
  std::vector<NodeVersionView> nodes;
  std::vector<EdgeVersionView> edges;
  IndexCoverage index_coverage;
  std::shared_ptr<LeaseRegistry> leases;
  std::map<std::string, std::vector<std::size_t>> label_index;
  std::map<std::string, std::vector<std::size_t>> property_index;
  std::map<NodeId, std::map<std::string, std::vector<std::size_t>>> out_adjacency;

  void rebuild_indexes() {
    label_index.clear();
    property_index.clear();
    out_adjacency.clear();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      label_index[nodes[i].label].push_back(i);
      for (const auto& [key, value] : nodes[i].properties) {
        property_index[property_index_key(key, value)].push_back(i);
      }
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
      out_adjacency[edges[i].from][edges[i].type].push_back(i);
    }
  }

  ~Impl() {
    if (leases) {
      leases->unpin(index_coverage.generation);
    }
  }
};

GraphSnapshot::GraphSnapshot() : impl_(std::make_shared<Impl>()) {}
GraphSnapshot::GraphSnapshot(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}

CommitSeq GraphSnapshot::commit() const {
  return impl_->commit;
}

std::int64_t GraphSnapshot::valid_time() const {
  return impl_->valid_time;
}

const std::vector<NodeVersionView>& GraphSnapshot::nodes() const {
  return impl_->nodes;
}

const std::vector<EdgeVersionView>& GraphSnapshot::edges() const {
  return impl_->edges;
}

std::optional<NodeVersionView> GraphSnapshot::node(NodeId id) const {
  const auto found = std::lower_bound(
      impl_->nodes.begin(), impl_->nodes.end(), id,
      [](const NodeVersionView& lhs, NodeId rhs) { return lhs.id < rhs; });
  if (found == impl_->nodes.end() || found->id != id) {
    return std::nullopt;
  }
  return *found;
}

std::vector<NodeVersionView> GraphSnapshot::nodes_with_label(std::string_view label) const {
  std::vector<NodeVersionView> result;
  const auto found = impl_->label_index.find(std::string(label));
  if (found == impl_->label_index.end()) {
    return result;
  }
  result.reserve(found->second.size());
  for (const auto index : found->second) {
    result.push_back(impl_->nodes[index]);
  }
  return result;
}

std::vector<NodeVersionView> GraphSnapshot::nodes_with_property(std::string_view key,
                                                                const PropertyValue& value) const {
  std::vector<NodeVersionView> result;
  const auto found = impl_->property_index.find(property_index_key(key, value));
  if (found == impl_->property_index.end()) {
    return result;
  }
  result.reserve(found->second.size());
  for (const auto index : found->second) {
    result.push_back(impl_->nodes[index]);
  }
  return result;
}

std::vector<EdgeVersionView> GraphSnapshot::out_edges(NodeId from, std::string_view type) const {
  std::vector<EdgeVersionView> result;
  const auto from_it = impl_->out_adjacency.find(from);
  if (from_it == impl_->out_adjacency.end()) {
    return result;
  }
  const auto type_it = from_it->second.find(std::string(type));
  if (type_it == from_it->second.end()) {
    return result;
  }
  result.reserve(type_it->second.size());
  for (const auto index : type_it->second) {
    result.push_back(impl_->edges[index]);
  }
  return result;
}

IndexCoverage GraphSnapshot::index_coverage() const {
  return impl_->index_coverage;
}

struct GraphStore::Impl : std::enable_shared_from_this<GraphStore::Impl> {
  explicit Impl(std::filesystem::path store_path, Limits configured_limits)
      : path(std::move(store_path)), limits(configured_limits) {}

  std::filesystem::path path;
  Limits limits;
  mutable std::mutex mutex;
  CommitSeq latest{};
  std::shared_ptr<GraphSnapshot::Impl::LeaseRegistry> leases{
      std::make_shared<GraphSnapshot::Impl::LeaseRegistry>()};
  VersionMap<NodeId, NodeRecord> nodes;
  VersionMap<EdgeId, EdgeRecord> edges;

  Result<GraphSnapshot> make_snapshot(SnapshotSelector selector) const {
    std::lock_guard<std::mutex> lock(mutex);
    const CommitSeq commit = selector.commit.value_or(latest);
    if (commit.value > latest.value) {
      return make_error(ErrorCode::NotFound, "requested commit is newer than store head", "store");
    }

    auto snapshot = std::make_shared<GraphSnapshot::Impl>();
    snapshot->commit = commit;
    snapshot->valid_time = selector.valid_time;
    snapshot->index_coverage =
        IndexCoverage{commit.value == 0 ? std::uint64_t{1} : commit.value, commit};
    snapshot->leases = leases;
    leases->pin(snapshot->index_coverage.generation);

    std::vector<NodeVersionView> commit_visible_nodes;
    for (const auto& [id, history] : nodes) {
      (void)history;
      const auto* record = latest_at(nodes, id, commit);
      if (record != nullptr && !record->tombstone) {
        commit_visible_nodes.push_back(record->view);
      }
    }
    snapshot->nodes = select_active_at(std::move(commit_visible_nodes), selector.valid_time,
                                       [](const NodeVersionView& node) { return node.id.value; });

    std::set<NodeId> active_nodes;
    for (const auto& node : snapshot->nodes) {
      active_nodes.insert(node.id);
    }

    std::vector<EdgeVersionView> commit_visible_edges;
    for (const auto& [id, history] : edges) {
      (void)history;
      const auto* record = latest_at(edges, id, commit);
      if (record != nullptr && !record->tombstone) {
        commit_visible_edges.push_back(record->view);
      }
    }
    auto active_edges = select_active_at(std::move(commit_visible_edges), selector.valid_time,
                                         [](const EdgeVersionView& edge) { return edge.id.value; });
    for (const auto& edge : active_edges) {
      if (active_nodes.contains(edge.from) && active_nodes.contains(edge.to)) {
        snapshot->edges.push_back(edge);
      }
    }

    std::sort(snapshot->nodes.begin(), snapshot->nodes.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
    std::sort(snapshot->edges.begin(), snapshot->edges.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
    snapshot->rebuild_indexes();
    return GraphSnapshot{snapshot};
  }

  Result<void> load() {
    auto image = ogr::read_store(path, limits);
    if (!image) {
      return image.error();
    }
    latest = image.value().latest_commit;
    leases->register_generation(latest.value == 0 ? std::uint64_t{1} : latest.value);
    for (const auto& node : image.value().node_versions) {
      nodes[node.id].push_back(NodeRecord{node, false});
    }
    for (const auto& node : image.value().node_tombstones) {
      nodes[node.id].push_back(NodeRecord{node, true});
    }
    for (const auto& edge : image.value().edge_versions) {
      edges[edge.id].push_back(EdgeRecord{edge, false});
    }
    for (const auto& edge : image.value().edge_tombstones) {
      edges[edge.id].push_back(EdgeRecord{edge, true});
    }
    for (auto& [id, history] : nodes) {
      (void)id;
      std::sort(history.begin(), history.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.view.begin_commit < rhs.view.begin_commit;
                });
    }
    for (auto& [id, history] : edges) {
      (void)id;
      std::sort(history.begin(), history.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.view.begin_commit < rhs.view.begin_commit;
                });
    }
    return {};
  }
};

struct Transaction::Impl {
  explicit Impl(std::shared_ptr<GraphStore::Impl> owner) : store(std::move(owner)) {}

  std::shared_ptr<GraphStore::Impl> store;
  std::map<NodeId, NodeVersionView> put_nodes;
  std::map<EdgeId, EdgeVersionView> put_edges;
  std::set<NodeId> delete_nodes;
  std::set<EdgeId> delete_edges;
  bool closed{false};

  Result<void> validate_node(NodeId id, const std::string& label, Interval interval,
                             const PropertyMap& properties) const {
    if (id.value == 0) {
      return invalid_id("node");
    }
    if (label.empty()) {
      return make_error(ErrorCode::Usage, "node label must not be empty", "txn");
    }
    auto interval_result = validate_interval(interval);
    if (!interval_result) {
      return interval_result;
    }
    if (properties.size() > store->limits.max_properties) {
      return make_error(ErrorCode::ResourceLimit, "too many node properties", "txn");
    }
    return {};
  }

  Result<void> validate_edge(EdgeId id, NodeId from, NodeId to, const std::string& type,
                             Interval interval, const PropertyMap& properties) const {
    if (id.value == 0) {
      return invalid_id("edge");
    }
    if (from.value == 0 || to.value == 0) {
      return invalid_id("endpoint");
    }
    if (type.empty()) {
      return make_error(ErrorCode::Usage, "edge type must not be empty", "txn");
    }
    auto interval_result = validate_interval(interval);
    if (!interval_result) {
      return interval_result;
    }
    if (properties.size() > store->limits.max_properties) {
      return make_error(ErrorCode::ResourceLimit, "too many edge properties", "txn");
    }
    return {};
  }
};

Transaction::Transaction() = default;
Transaction::Transaction(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Transaction::Transaction(Transaction&&) noexcept = default;
Transaction& Transaction::operator=(Transaction&&) noexcept = default;
Transaction::~Transaction() = default;

Result<void> Transaction::put_node(NodeId id, std::string label, Interval interval,
                                   PropertyMap properties) {
  if (!impl_ || impl_->closed) {
    return make_error(ErrorCode::Conflict, "transaction is closed", "txn");
  }
  auto validation = impl_->validate_node(id, label, interval, properties);
  if (!validation) {
    return validation;
  }
  impl_->delete_nodes.erase(id);
  impl_->put_nodes[id] = NodeVersionView{id, std::move(label), interval, std::move(properties), {}};
  return {};
}

Result<void> Transaction::put_edge(EdgeId id, NodeId from, NodeId to, std::string type,
                                   Interval interval, PropertyMap properties) {
  if (!impl_ || impl_->closed) {
    return make_error(ErrorCode::Conflict, "transaction is closed", "txn");
  }
  auto validation = impl_->validate_edge(id, from, to, type, interval, properties);
  if (!validation) {
    return validation;
  }
  impl_->delete_edges.erase(id);
  impl_->put_edges[id] =
      EdgeVersionView{id, from, to, std::move(type), interval, std::move(properties), {}};
  return {};
}

Result<void> Transaction::delete_node(NodeId id) {
  if (!impl_ || impl_->closed) {
    return make_error(ErrorCode::Conflict, "transaction is closed", "txn");
  }
  if (id.value == 0) {
    return invalid_id("node");
  }
  impl_->put_nodes.erase(id);
  impl_->delete_nodes.insert(id);
  return {};
}

Result<void> Transaction::delete_edge(EdgeId id) {
  if (!impl_ || impl_->closed) {
    return make_error(ErrorCode::Conflict, "transaction is closed", "txn");
  }
  if (id.value == 0) {
    return invalid_id("edge");
  }
  impl_->put_edges.erase(id);
  impl_->delete_edges.insert(id);
  return {};
}

Result<CommitSeq> Transaction::commit() {
  if (!impl_ || impl_->closed) {
    return make_error(ErrorCode::Conflict, "transaction is closed", "txn");
  }
  auto& store = *impl_->store;
  std::lock_guard<std::mutex> lock(store.mutex);
  const CommitSeq parent = store.latest;
  auto next_commit = checked_add(parent.value, 1U);
  if (!next_commit) {
    return next_commit.error();
  }
  const CommitSeq commit{next_commit.value()};

  std::set<NodeId> staged_available;
  for (const auto& [id, node] : impl_->put_nodes) {
    (void)node;
    staged_available.insert(id);
  }

  for (const auto& [id, edge] : impl_->put_edges) {
    (void)id;
    const auto* from = latest_at(store.nodes, edge.from, parent);
    const auto* to = latest_at(store.nodes, edge.to, parent);
    const bool from_ok = (from != nullptr && !from->tombstone) ||
                         staged_available.contains(edge.from);
    const bool to_ok = (to != nullptr && !to->tombstone) || staged_available.contains(edge.to);
    if (!from_ok || !to_ok || impl_->delete_nodes.contains(edge.from) ||
        impl_->delete_nodes.contains(edge.to)) {
      return make_error(ErrorCode::Conflict, "edge endpoint is not visible in transaction base",
                        "txn");
    }
  }

  std::vector<NodeVersionView> nodes;
  std::vector<EdgeVersionView> edges;
  std::vector<NodeId> node_tombstones(impl_->delete_nodes.begin(), impl_->delete_nodes.end());
  std::vector<EdgeId> edge_tombstones(impl_->delete_edges.begin(), impl_->delete_edges.end());

  for (auto [id, node] : impl_->put_nodes) {
    (void)id;
    node.begin_commit = commit;
    nodes.push_back(std::move(node));
  }
  for (auto [id, edge] : impl_->put_edges) {
    (void)id;
    edge.begin_commit = commit;
    edges.push_back(std::move(edge));
  }

  auto append = ogr::append_transaction(store.path, parent, commit, nodes, edges, node_tombstones,
                                        edge_tombstones, store.limits);
  if (!append) {
    return append.error();
  }

  for (const auto& node : nodes) {
    store.nodes[node.id].push_back(NodeRecord{node, false});
  }
  for (const auto& id : node_tombstones) {
    store.nodes[id].push_back(
        NodeRecord{NodeVersionView{id, "", Interval{0, 1}, {}, commit}, true});
  }
  for (const auto& edge : edges) {
    store.edges[edge.id].push_back(EdgeRecord{edge, false});
  }
  for (const auto& id : edge_tombstones) {
    store.edges[id].push_back(
        EdgeRecord{EdgeVersionView{id, NodeId{0}, NodeId{0}, "", Interval{0, 1}, {}, commit},
                   true});
  }
  store.latest = commit;
  store.leases->register_generation(commit.value);
  impl_->closed = true;
  return commit;
}

void Transaction::abort() noexcept {
  if (impl_) {
    impl_->closed = true;
  }
}

GraphStore::GraphStore() = default;
GraphStore::GraphStore(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

Result<GraphStore> GraphStore::open(const std::filesystem::path& path, OpenOptions options) {
  if (!std::filesystem::exists(path)) {
    if (!options.create_if_missing) {
      return make_error(ErrorCode::Io, "store does not exist", "store");
    }
    auto created = ogr::create_empty(path);
    if (!created) {
      return created.error();
    }
  }
  auto impl = std::make_shared<Impl>(path, options.limits);
  auto loaded = impl->load();
  if (!loaded) {
    return loaded.error();
  }
  return GraphStore{impl};
}

Result<Transaction> GraphStore::begin() {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized store", "store");
  }
  return Transaction{std::make_unique<Transaction::Impl>(impl_)};
}

Result<GraphSnapshot> GraphStore::snapshot(SnapshotSelector selector) const {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized store", "store");
  }
  return impl_->make_snapshot(selector);
}

Result<PreparedQuery> GraphStore::prepare(std::string_view query) const {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized store", "store");
  }
  return prepare_query(query, impl_->limits);
}

CommitSeq GraphStore::latest_commit() const {
  if (!impl_) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->latest;
}

CacheStats GraphStore::cache_stats() const {
  if (!impl_) {
    return {};
  }
  return impl_->leases->stats();
}

Result<std::size_t> GraphStore::evict_unpinned_indexes() {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized store", "store");
  }
  return impl_->leases->evict_unpinned();
}

Result<void> GraphStore::check() const {
  if (!impl_) {
    return make_error(ErrorCode::InternalInvariant, "uninitialized store", "store");
  }
  auto image = ogr::read_store(impl_->path, impl_->limits);
  if (!image) {
    return image.error();
  }
  return {};
}

std::string GraphStore::inspect() const {
  if (!impl_) {
    return "uninitialized\n";
  }
  auto snapshot_result = snapshot(SnapshotSelector{impl_->latest, 0});
  std::ostringstream out;
  out << "OGR1 latest_commit=" << impl_->latest.value << "\n";
  if (snapshot_result) {
    out << "visible_at_time_0 nodes=" << snapshot_result.value().nodes().size()
        << " edges=" << snapshot_result.value().edges().size() << "\n";
  }
  return out.str();
}

}  // namespace orbit
