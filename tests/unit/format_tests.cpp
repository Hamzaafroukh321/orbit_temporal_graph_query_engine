#include "test_support.hpp"

#include "orbit/format.hpp"

#include <fstream>
#include <vector>

namespace {

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_prefix(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes,
                  std::size_t size) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(size));
}

}  // namespace

ORBIT_TEST(EmptyStoreSuperblockChoice) {
  auto path = orbit::test::temp_path("empty.ogr");
  auto created = orbit::ogr::create_empty(path);
  REQUIRE(created);
  auto image = orbit::ogr::read_store(path);
  REQUIRE(image);
  REQUIRE(image.value().latest_commit.value == 0);
}

ORBIT_TEST(RecordRoundTripNodeVersion) {
  auto path = orbit::test::temp_path("roundtrip_node.ogr");
  REQUIRE(orbit::ogr::create_empty(path));
  orbit::NodeVersionView node{orbit::NodeId{100}, "Service", orbit::Interval{0, 10},
                              {{"name", std::string{"api"}}}, orbit::CommitSeq{1}};
  auto appended = orbit::ogr::append_transaction(path, orbit::CommitSeq{0}, orbit::CommitSeq{1},
                                                 {node}, {}, {}, {});
  REQUIRE(appended);
  auto image = orbit::ogr::read_store(path);
  REQUIRE(image);
  REQUIRE(image.value().node_versions.size() == 1);
  REQUIRE(image.value().node_versions[0].label == "Service");
}

ORBIT_TEST(TruncatedTxnInvisible) {
  auto path = orbit::test::temp_path("truncated.ogr");
  REQUIRE(orbit::ogr::create_empty(path));
  orbit::NodeVersionView node{orbit::NodeId{100}, "Service", orbit::Interval{0, 10}, {},
                              orbit::CommitSeq{1}};
  REQUIRE(orbit::ogr::append_transaction(path, orbit::CommitSeq{0}, orbit::CommitSeq{1}, {node},
                                         {}, {}, {}));
  auto size = std::filesystem::file_size(path);
  std::filesystem::resize_file(path, size - 20U);
  auto image = orbit::ogr::read_store(path);
  REQUIRE(image);
  REQUIRE(image.value().latest_commit.value == 0);
}

ORBIT_TEST(RecoveryEveryBytePrefixExposesCommittedPrefix) {
  auto path = orbit::test::temp_path("recovery_prefix_source.ogr");
  REQUIRE(orbit::ogr::create_empty(path));
  orbit::NodeVersionView first{orbit::NodeId{1}, "Service", orbit::Interval{0, 10}, {},
                               orbit::CommitSeq{1}};
  orbit::NodeVersionView second{orbit::NodeId{2}, "Database", orbit::Interval{0, 10}, {},
                                orbit::CommitSeq{2}};
  REQUIRE(orbit::ogr::append_transaction(path, orbit::CommitSeq{0}, orbit::CommitSeq{1}, {first},
                                         {}, {}, {}));
  const auto after_first = std::filesystem::file_size(path);
  REQUIRE(orbit::ogr::append_transaction(path, orbit::CommitSeq{1}, orbit::CommitSeq{2}, {second},
                                         {}, {}, {}));
  const auto full_size = std::filesystem::file_size(path);
  const auto full_bytes = read_bytes(path);
  const auto cut_path = orbit::test::temp_path("recovery_prefix_cut.ogr");

  for (std::size_t cut = 4096; cut <= full_size; ++cut) {
    write_prefix(cut_path, full_bytes, cut);
    auto image = orbit::ogr::read_store(cut_path);
    REQUIRE(image);
    const std::uint64_t expected =
        cut == full_size ? 2U : (cut >= after_first ? 1U : 0U);
    REQUIRE(image.value().latest_commit.value == expected);
    REQUIRE(image.value().node_versions.size() == expected);
  }
}

ORBIT_TEST(BadMagicRejectsStore) {
  auto path = orbit::test::temp_path("bad_magic.ogr");
  {
    std::ofstream out(path, std::ios::binary);
    out << "NOPE";
  }
  auto image = orbit::ogr::read_store(path);
  REQUIRE(!image);
}

ORBIT_TEST(InvalidIntervalRecordRejected) {
  auto path = orbit::test::temp_path("bad_interval.ogr");
  REQUIRE(orbit::ogr::create_empty(path));
  orbit::NodeVersionView node{orbit::NodeId{1}, "X", orbit::Interval{10, 1}, {},
                              orbit::CommitSeq{1}};
  auto appended = orbit::ogr::append_transaction(path, orbit::CommitSeq{0}, orbit::CommitSeq{1},
                                                 {node}, {}, {}, {});
  REQUIRE(!appended);
  auto image = orbit::ogr::read_store(path);
  REQUIRE(image);
  REQUIRE(image.value().latest_commit.value == 0);
}

ORBIT_TEST(Crc32cIsDeterministic) {
  std::vector<std::uint8_t> data{1, 2, 3, 4};
  REQUIRE(orbit::ogr::crc32c(data) == orbit::ogr::crc32c(data));
}
