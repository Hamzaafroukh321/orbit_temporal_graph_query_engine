#include "orbit/store.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

namespace {

std::vector<std::uint8_t> read_seed(const std::filesystem::path& base) {
  const auto dir = base / "sequences";
  if (std::filesystem::exists(dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        std::ifstream input(entry.path(), std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
      }
    }
  }
  return {1, 2, 3, 4, 5, 6, 7, 8};
}

struct RefNode {
  std::string label;
  orbit::Interval interval;
};

struct RefEdge {
  std::uint64_t from{0};
  std::uint64_t to{0};
  std::string type;
  orbit::Interval interval;
  std::int64_t weight{1};
};

struct RefGraph {
  std::map<std::uint64_t, RefNode> nodes;
  std::map<std::uint64_t, RefEdge> edges;
  std::uint64_t commits{0};

  void put_node(std::uint64_t id, std::string label, orbit::Interval interval) {
    nodes[id] = RefNode{std::move(label), interval};
    ++commits;
  }

  void put_edge(std::uint64_t id, std::uint64_t from, std::uint64_t to, std::string type,
                orbit::Interval interval, std::int64_t weight) {
    edges[id] = RefEdge{from, to, std::move(type), interval, weight};
    ++commits;
  }

  void delete_node(std::uint64_t id) {
    nodes.erase(id);
    ++commits;
  }

  void delete_edge(std::uint64_t id) {
    edges.erase(id);
    ++commits;
  }

  bool active_node(std::uint64_t id, std::int64_t time) const {
    const auto found = nodes.find(id);
    return found != nodes.end() && found->second.interval.contains(time);
  }

  std::vector<std::string> service_nodes(std::int64_t time) const {
    std::vector<std::string> rows;
    for (const auto& [id, node] : nodes) {
      if (node.label == "Service" && node.interval.contains(time)) {
        rows.push_back(std::to_string(id));
      }
    }
    return rows;
  }

  std::vector<std::string> service_steps(std::int64_t time) const {
    std::vector<std::string> rows;
    for (const auto& [seed_id, node] : nodes) {
      if (node.label != "Service" || !node.interval.contains(time)) {
        continue;
      }
      for (const auto& [edge_id, edge] : edges) {
        if (edge.from == seed_id && edge.type == "DEPENDS" && edge.interval.contains(time) &&
            active_node(edge.to, time)) {
          rows.push_back(std::to_string(edge.to));
        }
        (void)edge_id;
      }
    }
    return rows;
  }

  std::vector<std::string> service_paths(std::int64_t time) const {
    struct Emitted {
      double cost{0.0};
      std::string key;
      std::string path;
    };

    std::vector<Emitted> emitted;
    for (const auto& [seed_id, node] : nodes) {
      if (node.label != "Service" || !node.interval.contains(time)) {
        continue;
      }
      struct Frontier {
        std::uint64_t node{0};
        std::vector<std::uint64_t> path;
        std::size_t hops{0};
        double cost{0.0};
      };
      std::vector<Frontier> queue{Frontier{seed_id, {seed_id}, 0, 0.0}};
      for (std::size_t offset = 0; offset < queue.size(); ++offset) {
        const auto current = queue[offset];
        if (current.hops >= 2U) {
          continue;
        }
        for (const auto& [edge_id, edge] : edges) {
          if (edge.from != current.node || edge.type != "DEPENDS" ||
              !edge.interval.contains(time) || !active_node(edge.to, time)) {
            continue;
          }
          if (std::find(current.path.begin(), current.path.end(), edge.to) != current.path.end()) {
            continue;
          }
          auto next_path = current.path;
          next_path.push_back(edge.to);
          std::ostringstream path_text;
          for (std::size_t i = 0; i < next_path.size(); ++i) {
            if (i != 0) {
              path_text << "->";
            }
            path_text << next_path[i];
          }
          const auto path = path_text.str();
          const auto cost = current.cost + static_cast<double>(edge.weight);
          emitted.push_back(Emitted{cost, "path:" + path, path});
          queue.push_back(Frontier{edge.to, std::move(next_path), current.hops + 1U, cost});
          (void)edge_id;
        }
      }
    }
    std::sort(emitted.begin(), emitted.end(), [](const auto& lhs, const auto& rhs) {
      if (lhs.cost != rhs.cost) {
        return lhs.cost < rhs.cost;
      }
      return lhs.key < rhs.key;
    });
    std::vector<std::string> rows;
    rows.reserve(emitted.size());
    for (const auto& row : emitted) {
      rows.push_back(row.path);
    }
    return rows;
  }
};

std::vector<std::string> drain_rows(orbit::ResultCursor cursor) {
  std::vector<std::string> rows;
  while (true) {
    auto batch = cursor.next(7);
    if (!batch) {
      return {"error:" + batch.error().describe()};
    }
    if (!batch.value()) {
      break;
    }
    for (const auto& row : batch.value()->rows) {
      if (row.values.empty()) {
        return {"error:bad-row"};
      }
      if (std::holds_alternative<std::int64_t>(row.values[0])) {
        rows.push_back(std::to_string(std::get<std::int64_t>(row.values[0])));
      } else if (std::holds_alternative<std::string>(row.values[0])) {
        rows.push_back(std::get<std::string>(row.values[0]));
      } else {
        return {"error:bad-row"};
      }
    }
  }
  return rows;
}

bool compare_query(orbit::GraphStore& store, const orbit::GraphSnapshot& snapshot,
                   const std::string& text, std::vector<std::string> expected) {
  auto query = store.prepare(text);
  if (!query) {
    std::cerr << "failed to prepare reference comparison: " << text << "\n";
    return false;
  }
  auto cursor = query.value().execute(snapshot, orbit::QueryLimits{256, 16, 4, 256, 10000});
  if (!cursor) {
    std::cerr << cursor.error().describe() << "\n";
    return false;
  }
  const auto production = drain_rows(std::move(cursor.value()));
  if (production != expected) {
    std::cerr << "reference mismatch query=" << text << " production=" << production.size()
              << " expected=" << expected.size() << "\n";
    return false;
  }
  return true;
}

bool compare_with_reference(orbit::GraphStore& store, const RefGraph& reference) {
  if (store.latest_commit().value != reference.commits) {
    std::cerr << "commit mismatch production=" << store.latest_commit().value
              << " reference=" << reference.commits << "\n";
    return false;
  }
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 1});
  if (!snapshot) {
    std::cerr << "failed to prepare reference snapshot\n";
    return false;
  }
  if (!compare_query(store, snapshot.value(), "FROM Service YIELD node.id",
                     reference.service_nodes(1))) {
    return false;
  }
  if (!compare_query(store, snapshot.value(), "FROM Service STEP OUT DEPENDS YIELD node.id",
                     reference.service_steps(1))) {
    return false;
  }
  if (!compare_query(store, snapshot.value(),
                     "FROM Service PATH OUT DEPENDS HOPS 2 COST weight YIELD path",
                     reference.service_paths(1))) {
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
  const auto path = base / "sequences" / "fuzz_sequence.ogr";
  std::filesystem::create_directories(path.parent_path());
  std::filesystem::remove(path);
  auto store = orbit::GraphStore::open(path, orbit::OpenOptions{true});
  if (!store) {
    std::cerr << store.error().describe() << "\n";
    return 1;
  }

  const auto seed = read_seed(base);
  std::uint64_t next_node = 1;
  std::uint64_t next_edge = 1;
  RefGraph reference;
  const auto ops = std::min<std::size_t>(seed.size(), 128);
  for (std::size_t i = 0; i < ops; ++i) {
    auto txn = store.value().begin();
    if (!txn) {
      break;
    }
    const auto op = seed[i] % 5U;
    enum class Applied {
      None,
      PutNode,
      PutEdge,
      DeleteNode,
      DeleteEdge,
    };
    Applied applied = Applied::None;
    std::uint64_t applied_id = 0;
    std::uint64_t applied_to = 0;
    orbit::Interval applied_interval{0, 1};
    if (op == 0 || next_node < 3) {
      applied_id = next_node;
      applied_interval = orbit::Interval{0, 100 + static_cast<std::int64_t>(i)};
      (void)txn.value().put_node(orbit::NodeId{applied_id}, "Service", applied_interval);
      ++next_node;
      applied = Applied::PutNode;
    } else if (op == 1) {
      const auto from = orbit::NodeId{1};
      const auto to = orbit::NodeId{std::max<std::uint64_t>(2, (seed[i] % (next_node - 1U)) + 1U)};
      applied_id = next_edge;
      applied_to = to.value;
      (void)txn.value().put_edge(orbit::EdgeId{applied_id}, from, to, "DEPENDS",
                                 orbit::Interval{0, 100}, {{"weight", std::int64_t{1}}});
      ++next_edge;
      applied = Applied::PutEdge;
    } else if (op == 2) {
      applied_id = next_edge;
      (void)txn.value().delete_edge(orbit::EdgeId{applied_id});
      applied = Applied::DeleteEdge;
    } else if (op == 3) {
      applied_id = next_node + 100U;
      (void)txn.value().delete_node(orbit::NodeId{applied_id});
      applied = Applied::DeleteNode;
    } else {
      txn.value().abort();
      if (!compare_with_reference(store.value(), reference)) {
        return 1;
      }
      continue;
    }
    auto committed = txn.value().commit();
    if (committed) {
      switch (applied) {
        case Applied::PutNode:
          reference.put_node(applied_id, "Service", applied_interval);
          break;
        case Applied::PutEdge:
          reference.put_edge(applied_id, 1, applied_to, "DEPENDS", orbit::Interval{0, 100}, 1);
          break;
        case Applied::DeleteNode:
          reference.delete_node(applied_id);
          break;
        case Applied::DeleteEdge:
          reference.delete_edge(applied_id);
          break;
        case Applied::None:
          break;
      }
    }
    if (!compare_with_reference(store.value(), reference)) {
      return 1;
    }
    auto reopened = orbit::GraphStore::open(path);
    if (!reopened) {
      std::cerr << reopened.error().describe() << "\n";
      return 1;
    }
    if (!compare_with_reference(reopened.value(), reference)) {
      return 1;
    }
  }
  std::filesystem::remove(path);
  return 0;
}
