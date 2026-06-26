#include "orbit/store.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

struct TimedRows {
  std::size_t rows{0};
  double seconds{0.0};
};

double elapsed_seconds(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration<double>(end - start).count();
}

orbit::Result<std::size_t> drain_rows(orbit::ResultCursor& cursor) {
  std::size_t rows = 0;
  while (true) {
    auto batch = cursor.next(1024);
    if (!batch) {
      return batch.error();
    }
    if (!batch.value()) {
      break;
    }
    rows += batch.value()->rows.size();
  }
  return rows;
}

orbit::Result<TimedRows> timed_query(orbit::GraphStore& store, const orbit::GraphSnapshot& snapshot,
                                     std::string_view query) {
  auto prepared = store.prepare(query);
  if (!prepared) {
    return prepared.error();
  }
  auto start = Clock::now();
  auto cursor = prepared.value().execute(snapshot, orbit::QueryLimits{10000, 1024, 4, 10000, 1000000});
  if (!cursor) {
    return cursor.error();
  }
  auto rows = drain_rows(cursor.value());
  if (!rows) {
    return rows.error();
  }
  auto end = Clock::now();
  return TimedRows{rows.value(), elapsed_seconds(start, end)};
}

void write_metric(std::ostream& out, std::string_view key, std::uint64_t value) {
  out << key << '=' << value << '\n';
}

void write_metric(std::ostream& out, std::string_view key, double value) {
  out << key << '=' << value << '\n';
}

int fail(const orbit::Error& error) {
  std::cerr << error.describe() << '\n';
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  bool check_smoke_budgets = false;
  std::optional<std::filesystem::path> output_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--check-smoke-budgets") {
      check_smoke_budgets = true;
    } else if (arg == "--output" && i + 1 < argc) {
      output_path = std::filesystem::path(argv[++i]);
    } else {
      std::cerr << "usage: orbit_benchmark [--check-smoke-budgets] [--output file]\n";
      return 2;
    }
  }

  const auto bench_dir = std::filesystem::path("build") / "manual" / "bench";
  std::filesystem::create_directories(bench_dir);
  const auto store_path = bench_dir / "benchmark.ogr";
  std::error_code ignored;
  std::filesystem::remove(store_path, ignored);

  auto store_result = orbit::GraphStore::open(store_path, orbit::OpenOptions{true});
  if (!store_result) {
    return fail(store_result.error());
  }
  auto store = store_result.value();

  constexpr std::uint64_t kLeafCount = 1000;
  auto mutation_start = Clock::now();
  auto node_txn = store.begin();
  if (!node_txn) {
    return fail(node_txn.error());
  }
  auto put_hub = node_txn.value().put_node(orbit::NodeId{1}, "Hub", orbit::Interval{0, 100},
                                           {{"role", std::string{"root"}}});
  if (!put_hub) {
    return fail(put_hub.error());
  }
  for (std::uint64_t i = 0; i < kLeafCount; ++i) {
    auto put = node_txn.value().put_node(orbit::NodeId{i + 2U}, "Leaf", orbit::Interval{0, 100},
                                         {{"ordinal", static_cast<std::int64_t>(i)}});
    if (!put) {
      return fail(put.error());
    }
  }
  auto node_commit = node_txn.value().commit();
  if (!node_commit) {
    return fail(node_commit.error());
  }

  auto edge_txn = store.begin();
  if (!edge_txn) {
    return fail(edge_txn.error());
  }
  for (std::uint64_t i = 0; i < kLeafCount; ++i) {
    auto put = edge_txn.value().put_edge(orbit::EdgeId{i + 1U}, orbit::NodeId{1},
                                         orbit::NodeId{i + 2U}, "DEPENDS",
                                         orbit::Interval{0, 100},
                                         {{"weight", static_cast<std::int64_t>(i + 1U)}});
    if (!put) {
      return fail(put.error());
    }
  }
  auto edge_commit = edge_txn.value().commit();
  if (!edge_commit) {
    return fail(edge_commit.error());
  }
  auto mutation_end = Clock::now();

  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  if (!snapshot) {
    return fail(snapshot.error());
  }

  constexpr std::uint64_t kPointLookups = 20000;
  std::uint64_t point_checksum = 0;
  auto lookup_start = Clock::now();
  for (std::uint64_t i = 0; i < kPointLookups; ++i) {
    const auto id = orbit::NodeId{(i % kLeafCount) + 2U};
    auto node = snapshot.value().node(id);
    if (node) {
      point_checksum += node->id.value;
    }
  }
  auto lookup_end = Clock::now();

  auto one_hop = timed_query(store, snapshot.value(), "FROM Hub STEP OUT DEPENDS YIELD node.id");
  if (!one_hop) {
    return fail(one_hop.error());
  }
  auto path = timed_query(store, snapshot.value(), "FROM Hub PATH OUT DEPENDS HOPS 2 COST weight YIELD path");
  if (!path) {
    return fail(path.error());
  }

  const auto entity_versions = kLeafCount + 1U + kLeafCount;
  const auto mutation_seconds = elapsed_seconds(mutation_start, mutation_end);
  const auto lookup_seconds = elapsed_seconds(lookup_start, lookup_end);
  const auto mutation_per_second = static_cast<double>(entity_versions) / mutation_seconds;
  const auto lookup_mean_usec = (lookup_seconds * 1000000.0) / static_cast<double>(kPointLookups);
  const auto one_hop_per_second = static_cast<double>(one_hop.value().rows) / one_hop.value().seconds;
  const auto path_rows_per_second = static_cast<double>(path.value().rows) / path.value().seconds;

  std::ofstream file;
  std::ostream* out = &std::cout;
  if (output_path) {
    file.open(*output_path, std::ios::trunc);
    if (!file) {
      std::cerr << "failed to open benchmark output\n";
      return 1;
    }
    out = &file;
  }

  write_metric(*out, "entity_versions", entity_versions);
  write_metric(*out, "mutation_seconds", mutation_seconds);
  write_metric(*out, "mutation_versions_per_second", mutation_per_second);
  write_metric(*out, "point_lookups", kPointLookups);
  write_metric(*out, "point_lookup_mean_usec", lookup_mean_usec);
  write_metric(*out, "point_lookup_checksum", point_checksum);
  write_metric(*out, "one_hop_rows", static_cast<std::uint64_t>(one_hop.value().rows));
  write_metric(*out, "one_hop_seconds", one_hop.value().seconds);
  write_metric(*out, "one_hop_rows_per_second", one_hop_per_second);
  write_metric(*out, "path_rows", static_cast<std::uint64_t>(path.value().rows));
  write_metric(*out, "path_seconds", path.value().seconds);
  write_metric(*out, "path_rows_per_second", path_rows_per_second);

  if (check_smoke_budgets) {
    const bool ok = mutation_per_second >= 1000.0 && lookup_mean_usec <= 100.0 &&
                    one_hop_per_second >= 1000.0 && path_rows_per_second >= 100.0;
    if (!ok) {
      std::cerr << "benchmark smoke budget failed\n";
      return 1;
    }
  }
  return 0;
}
