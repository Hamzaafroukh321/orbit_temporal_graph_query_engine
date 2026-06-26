#include "orbit/format.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> read_seed(const std::filesystem::path& base) {
  const auto dir = base / "ogr";
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

bool write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  return output.good();
}

}  // namespace

int main(int argc, char** argv) {
  const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
  const auto dir = base / "ogr";
  const auto path = dir / "fuzz_smoke.ogr";
  std::filesystem::create_directories(dir);
  std::filesystem::remove(path);

  auto created = orbit::ogr::create_empty(path);
  if (!created) {
    std::cerr << created.error().describe() << "\n";
    return 1;
  }
  orbit::NodeVersionView node{orbit::NodeId{1}, "Fuzz", orbit::Interval{0, 20}, {},
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

  std::ifstream input(path, std::ios::binary);
  const std::vector<std::uint8_t> canonical{std::istreambuf_iterator<char>(input),
                                            std::istreambuf_iterator<char>()};
  const auto seed = read_seed(base);
  const auto variants = std::min<std::size_t>(seed.size(), 64);
  for (std::size_t i = 0; i < variants; ++i) {
    auto mutated = canonical;
    const auto cut = static_cast<std::size_t>(seed[i]) % (canonical.size() + 1U);
    mutated.resize(cut);
    if (!write_bytes(path, mutated)) {
      std::cerr << "failed to write mutated OGR input\n";
      return 1;
    }
    auto reopened = orbit::ogr::read_store(path);
    if (reopened && reopened.value().latest_commit.value > 1) {
      std::cerr << "mutated input exposed impossible commit\n";
      return 1;
    }
  }
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  return 0;
}
