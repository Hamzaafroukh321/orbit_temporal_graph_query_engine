#include "orbit/store.hpp"

#include <filesystem>
#include <iostream>

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
  (void)txn.value().commit();
  auto prepared = store.value().prepare("FROM A WHERE k = 7 YIELD node.id");
  auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, 1});
  if (!prepared || !snapshot) {
    std::cerr << "query smoke setup failed\n";
    return 1;
  }
  auto cursor = prepared.value().execute(snapshot.value());
  if (!cursor) {
    std::cerr << cursor.error().describe() << "\n";
    return 1;
  }
  auto batch = cursor.value().next(1);
  if (!batch || !batch.value() || batch.value()->rows.size() != 1) {
    std::cerr << "query smoke mismatch\n";
    return 1;
  }
  std::filesystem::remove(path);
  return 0;
}
