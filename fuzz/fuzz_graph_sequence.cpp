#include "orbit/store.hpp"

#include <filesystem>
#include <iostream>

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
  auto txn = store.value().begin();
  if (!txn) {
    std::cerr << txn.error().describe() << "\n";
    return 1;
  }
  (void)txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100});
  (void)txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 100});
  (void)txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                             orbit::Interval{0, 100});
  auto commit = txn.value().commit();
  if (!commit) {
    std::cerr << commit.error().describe() << "\n";
    return 1;
  }
  auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, 1});
  auto query = store.value().prepare("FROM Service STEP OUT DEPENDS YIELD node.id");
  if (!snapshot || !query) {
    std::cerr << "failed sequence smoke setup\n";
    return 1;
  }
  auto cursor = query.value().execute(snapshot.value());
  if (!cursor) {
    std::cerr << cursor.error().describe() << "\n";
    return 1;
  }
  auto batch = cursor.value().next(8);
  if (!batch || !batch.value() || batch.value()->rows.size() != 1) {
    std::cerr << "sequence smoke mismatch\n";
    return 1;
  }
  std::filesystem::remove(path);
  return 0;
}
