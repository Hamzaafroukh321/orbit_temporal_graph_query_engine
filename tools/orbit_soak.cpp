#include "orbit/store.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct SoakStats {
  std::uint64_t cycles{25};
  std::uint64_t commits{0};
  std::uint64_t compaction_attempts{0};
  std::uint64_t compaction_successes{0};
  std::uint64_t compaction_blocked{0};
  std::uint64_t reopens{0};
  std::uint64_t held_snapshot_checks{0};
  std::uint64_t final_rows{0};
};

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

orbit::Result<std::size_t> query_rows(orbit::GraphStore& store, const orbit::GraphSnapshot& snapshot) {
  auto prepared = store.prepare("FROM Root STEP OUT DEPENDS YIELD node.id");
  if (!prepared) {
    return prepared.error();
  }
  auto cursor = prepared.value().execute(snapshot, orbit::QueryLimits{100000, 1024, 4, 100000, 1000000});
  if (!cursor) {
    return cursor.error();
  }
  return drain_rows(cursor.value());
}

orbit::Result<void> assert_rows(orbit::GraphStore& store, const orbit::GraphSnapshot& snapshot,
                                std::size_t expected_rows) {
  auto rows = query_rows(store, snapshot);
  if (!rows) {
    return rows.error();
  }
  if (rows.value() != expected_rows) {
    return orbit::make_error(orbit::ErrorCode::InternalInvariant, "unexpected soak query row count",
                             "soak");
  }
  return {};
}

int fail(const orbit::Error& error) {
  std::cerr << error.describe() << '\n';
  return 1;
}

void write_stat(std::ostream& out, std::string_view key, std::uint64_t value) {
  out << key << '=' << value << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  SoakStats stats;
  std::optional<std::filesystem::path> output_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--cycles" && i + 1 < argc) {
      stats.cycles = static_cast<std::uint64_t>(std::stoull(argv[++i]));
    } else if (arg == "--output" && i + 1 < argc) {
      output_path = std::filesystem::path(argv[++i]);
    } else {
      std::cerr << "usage: orbit_soak [--cycles n] [--output file]\n";
      return 2;
    }
  }
  if (stats.cycles == 0 || stats.cycles > 10000U) {
    std::cerr << "cycles must be in 1..10000\n";
    return 2;
  }

  const auto soak_dir = std::filesystem::path("build") / "manual" / "soak";
  std::filesystem::create_directories(soak_dir);
  const auto store_path = soak_dir / "soak.ogr";
  std::error_code ignored;
  std::filesystem::remove(store_path, ignored);

  auto store_result = orbit::GraphStore::open(store_path, orbit::OpenOptions{true});
  if (!store_result) {
    return fail(store_result.error());
  }
  auto store = store_result.value();

  auto seed = store.begin();
  if (!seed) {
    return fail(seed.error());
  }
  auto root = seed.value().put_node(orbit::NodeId{1}, "Root", orbit::Interval{0, 1000});
  if (!root) {
    return fail(root.error());
  }
  for (std::uint64_t i = 0; i < stats.cycles; ++i) {
    auto node = seed.value().put_node(orbit::NodeId{i + 2U}, "Leaf", orbit::Interval{0, 1000},
                                      {{"ordinal", static_cast<std::int64_t>(i)}});
    if (!node) {
      return fail(node.error());
    }
  }
  auto seed_commit = seed.value().commit();
  if (!seed_commit) {
    return fail(seed_commit.error());
  }
  stats.commits = 1;

  std::vector<orbit::GraphSnapshot> held_snapshots;
  for (std::uint64_t cycle = 1; cycle <= stats.cycles; ++cycle) {
    auto txn = store.begin();
    if (!txn) {
      return fail(txn.error());
    }
    auto edge = txn.value().put_edge(orbit::EdgeId{cycle}, orbit::NodeId{1}, orbit::NodeId{cycle + 1U},
                                     "DEPENDS", orbit::Interval{0, 1000},
                                     {{"weight", static_cast<std::int64_t>(cycle)}});
    if (!edge) {
      return fail(edge.error());
    }
    auto commit = txn.value().commit();
    if (!commit) {
      return fail(commit.error());
    }
    ++stats.commits;

    auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
    if (!snapshot) {
      return fail(snapshot.error());
    }
    auto checked = assert_rows(store, snapshot.value(), static_cast<std::size_t>(cycle));
    if (!checked) {
      return fail(checked.error());
    }
    stats.final_rows = cycle;

    if ((cycle % 5U) == 0U) {
      held_snapshots.push_back(snapshot.value());
      if (held_snapshots.size() > 4U) {
        held_snapshots.erase(held_snapshots.begin());
      }
    }

    ++stats.compaction_attempts;
    const auto before = store.cache_stats();
    auto compacted = store.compact(2);
    const auto after = store.cache_stats();
    if (compacted) {
      ++stats.compaction_successes;
    } else {
      ++stats.compaction_blocked;
      if (before.total_pins != after.total_pins || before.pinned_generations != after.pinned_generations) {
        return fail(orbit::make_error(orbit::ErrorCode::InternalInvariant,
                                      "failed compaction changed cache pin accounting", "soak"));
      }
    }

    for (const auto& held : held_snapshots) {
      auto held_check = assert_rows(store, held, static_cast<std::size_t>(held.commit().value - 1U));
      if (!held_check) {
        return fail(held_check.error());
      }
      ++stats.held_snapshot_checks;
    }

    if ((cycle % 7U) == 0U) {
      held_snapshots.clear();
      auto reopened = orbit::GraphStore::open(store_path);
      if (!reopened) {
        return fail(reopened.error());
      }
      store = reopened.value();
      ++stats.reopens;
      if (store.latest_commit().value != cycle + 1U) {
        return fail(orbit::make_error(orbit::ErrorCode::InternalInvariant,
                                      "reopen exposed wrong latest commit", "soak"));
      }
      auto reopened_snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
      if (!reopened_snapshot) {
        return fail(reopened_snapshot.error());
      }
      auto reopened_check = assert_rows(store, reopened_snapshot.value(), static_cast<std::size_t>(cycle));
      if (!reopened_check) {
        return fail(reopened_check.error());
      }
    }
  }

  std::ofstream file;
  std::ostream* out = &std::cout;
  if (output_path) {
    file.open(*output_path, std::ios::trunc);
    if (!file) {
      std::cerr << "failed to open soak output\n";
      return 1;
    }
    out = &file;
  }
  write_stat(*out, "cycles", stats.cycles);
  write_stat(*out, "commits", stats.commits);
  write_stat(*out, "compaction_attempts", stats.compaction_attempts);
  write_stat(*out, "compaction_successes", stats.compaction_successes);
  write_stat(*out, "compaction_blocked", stats.compaction_blocked);
  write_stat(*out, "reopens", stats.reopens);
  write_stat(*out, "held_snapshot_checks", stats.held_snapshot_checks);
  write_stat(*out, "final_rows", stats.final_rows);
  return 0;
}
