#include "orbit/store.hpp"

#include <filesystem>
#include <iostream>

int main() {
  const auto path = std::filesystem::path("build") / "manual" / "package" / "consumer_graph.ogr";
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  auto store = orbit::GraphStore::open(path, orbit::OpenOptions{true});
  if (!store) {
    std::cerr << store.error().describe() << '\n';
    return 1;
  }
  auto txn = store.value().begin();
  if (!txn) {
    std::cerr << txn.error().describe() << '\n';
    return 1;
  }
  if (!txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}) ||
      !txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 100}) ||
      !txn.value().put_edge(orbit::EdgeId{10}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                            orbit::Interval{0, 100})) {
    std::cerr << "failed to stage graph\n";
    return 1;
  }
  auto commit = txn.value().commit();
  if (!commit) {
    std::cerr << commit.error().describe() << '\n';
    return 1;
  }
  auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto query = store.value().prepare("FROM Service STEP OUT DEPENDS YIELD node.id");
  if (!snapshot || !query) {
    std::cerr << "failed to prepare consumer query\n";
    return 1;
  }
  auto cursor = query.value().execute(snapshot.value());
  if (!cursor) {
    std::cerr << cursor.error().describe() << '\n';
    return 1;
  }
  auto batch = cursor.value().next(10);
  if (!batch || !batch.value()) {
    std::cerr << "failed to read consumer result\n";
    return 1;
  }
  std::cout << "consumer_rows=" << batch.value()->rows.size() << '\n';
  return batch.value()->rows.size() == 1U ? 0 : 1;
}
