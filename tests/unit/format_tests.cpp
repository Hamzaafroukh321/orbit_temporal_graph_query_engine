#include "test_support.hpp"

#include "orbit/format.hpp"

#include <fstream>

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
