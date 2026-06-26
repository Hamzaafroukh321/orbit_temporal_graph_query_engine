#include "orbit/format.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
  const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
  const auto path = base / "ogr" / "fuzz_smoke.ogr";
  std::filesystem::create_directories(path.parent_path());
  auto created = orbit::ogr::create_empty(path);
  if (!created) {
    std::cerr << created.error().describe() << "\n";
    return 1;
  }
  orbit::NodeVersionView node{orbit::NodeId{1}, "Fuzz", orbit::Interval{0, 2}, {},
                              orbit::CommitSeq{1}};
  auto appended = orbit::ogr::append_transaction(path, orbit::CommitSeq{0}, orbit::CommitSeq{1},
                                                 {node}, {}, {}, {});
  if (!appended) {
    std::cerr << appended.error().describe() << "\n";
    return 1;
  }
  auto image = orbit::ogr::read_store(path);
  if (!image || image.value().latest_commit.value != 1) {
    std::cerr << (image ? "bad latest commit" : image.error().describe()) << "\n";
    return 1;
  }
  std::filesystem::remove(path);
  return 0;
}
