#include "orbit/store.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  const auto ops = std::min<std::size_t>(seed.size(), 128);
  for (std::size_t i = 0; i < ops; ++i) {
    auto txn = store.value().begin();
    if (!txn) {
      break;
    }
    const auto op = seed[i] % 5U;
    if (op == 0 || next_node < 3) {
      (void)txn.value().put_node(orbit::NodeId{next_node}, "Service",
                                 orbit::Interval{0, 100 + static_cast<std::int64_t>(i)});
      ++next_node;
    } else if (op == 1) {
      const auto from = orbit::NodeId{1};
      const auto to = orbit::NodeId{std::max<std::uint64_t>(2, (seed[i] % (next_node - 1U)) + 1U)};
      (void)txn.value().put_edge(orbit::EdgeId{next_edge}, from, to, "DEPENDS",
                                 orbit::Interval{0, 100}, {{"weight", std::int64_t{1}}});
      ++next_edge;
    } else if (op == 2) {
      (void)txn.value().delete_edge(orbit::EdgeId{next_edge});
    } else if (op == 3) {
      (void)txn.value().delete_node(orbit::NodeId{next_node + 100U});
    } else {
      txn.value().abort();
      continue;
    }
    (void)txn.value().commit();

    auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, 1});
    auto query = store.value().prepare("FROM Service PATH OUT DEPENDS HOPS 2 COST weight YIELD path");
    if (snapshot && query) {
      auto cursor = query.value().execute(snapshot.value(), orbit::QueryLimits{256, 16, 4, 256, 10000});
      if (cursor) {
        (void)cursor.value().next(3);
      }
    }
    auto reopened = orbit::GraphStore::open(path);
    if (!reopened) {
      std::cerr << reopened.error().describe() << "\n";
      return 1;
    }
  }
  std::filesystem::remove(path);
  return 0;
}
