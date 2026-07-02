#include "orbit/store.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> read_seed(const std::filesystem::path& base) {
  const auto dir = base / "queries";
  if (std::filesystem::exists(dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        std::ifstream input(entry.path(), std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
      }
    }
  }
  return {0, 1, 2, 3, 4, 5, 6, 7};
}

}  // namespace

int main(int argc, char** argv) {
  const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
  const auto path = base / "queries" / "fuzz_query.ogr";
  std::filesystem::create_directories(path.parent_path());
  std::filesystem::remove(path);
  auto store = orbit::GraphStore::open(path, orbit::OpenOptions{true});
  if (!store) {
    std::cerr << store.error().describe() << "\n";
    return 1;
  }
  auto txn = store.value().begin();
  (void)txn.value().put_node(orbit::NodeId{1}, "A", orbit::Interval{0, 10},
                             {{"k", std::int64_t{7}}});
  (void)txn.value().put_node(orbit::NodeId{2}, "B", orbit::Interval{0, 10});
  (void)txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "E",
                             orbit::Interval{0, 10}, {{"w", std::int64_t{2}}});
  (void)txn.value().commit();

  const std::array<const char*, 10> queries{
      "FROM A WHERE k = 7 YIELD node.id",
      "FROM A WHERE k >= 7 YIELD node.id",
      "FROM A WHERE k != 9 YIELD node.id",
      "FROM A STEP OUT E YIELD node.id",
      "FROM B STEP IN E YIELD node.id",
      "FROM A PATH OUT E HOPS 2 YIELD path",
      "FROM B PATH IN E HOPS 2 YIELD path",
      "FROM A PATH OUT E HOPS 2 COST w YIELD path",
      "FROM Missing YIELD node.id",
      "FROM A PATH OUT E HOPS 99 YIELD path"};
  const auto seed = read_seed(base);
  for (std::size_t i = 0; i < std::min<std::size_t>(seed.size(), 64); ++i) {
    auto prepared = store.value().prepare(queries[seed[i] % queries.size()]);
    if (!prepared) {
      continue;
    }
    auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, 1});
    if (!snapshot) {
      std::cerr << snapshot.error().describe() << "\n";
      return 1;
    }
    orbit::CancelToken cancel;
    if ((seed[i] % 11U) == 0) {
      cancel.cancel();
    }
    auto cursor =
        prepared.value().execute(snapshot.value(), orbit::QueryLimits{128, 16, 32, 128, 1000}, cancel);
    if (cursor) {
      const auto batch_size = static_cast<std::size_t>((seed[i] % 5U) + 1U);
      (void)cursor.value().next(batch_size);
    }
  }
  std::filesystem::remove(path);
  return 0;
}
