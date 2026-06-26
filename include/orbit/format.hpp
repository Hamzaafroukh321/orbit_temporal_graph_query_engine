#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "orbit/error.hpp"
#include "orbit/store.hpp"

namespace orbit::ogr {

enum class RecordType : std::uint16_t {
  TxnBegin = 0x0001,
  NodeVersion = 0x0010,
  EdgeVersion = 0x0011,
  TxnCommit = 0x0030,
};

struct StoreImage {
  std::uint64_t generation{0};
  CommitSeq latest_commit{};
  std::vector<NodeVersionView> node_versions;
  std::vector<EdgeVersionView> edge_versions;
  std::vector<NodeVersionView> node_tombstones;
  std::vector<EdgeVersionView> edge_tombstones;
};

[[nodiscard]] std::uint32_t crc32c(const std::vector<std::uint8_t>& bytes) noexcept;
[[nodiscard]] Result<void> create_empty(const std::filesystem::path& path);
[[nodiscard]] Result<StoreImage> read_store(const std::filesystem::path& path, Limits limits = {});
[[nodiscard]] Result<void> append_transaction(const std::filesystem::path& path,
                                              CommitSeq parent,
                                              CommitSeq commit,
                                              const std::vector<NodeVersionView>& nodes,
                                              const std::vector<EdgeVersionView>& edges,
                                              const std::vector<NodeId>& node_tombstones,
                                              const std::vector<EdgeId>& edge_tombstones,
                                              Limits limits = {});

}  // namespace orbit::ogr
