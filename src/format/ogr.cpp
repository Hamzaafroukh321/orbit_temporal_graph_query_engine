#include "orbit/format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace orbit::ogr {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'O', 'G', 'R', '1'};
constexpr std::size_t kHeaderBytes = 4096;
constexpr std::uint16_t kMajor = 1;
constexpr std::uint16_t kMinor = 0;
constexpr std::uint16_t kSchema = 1;
constexpr std::size_t kRecordHeaderBytes = 40;

void put_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
  out.push_back(value);
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xffU));
  out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (int shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

void put_i64(std::vector<std::uint8_t>& out, std::int64_t value) {
  put_u64(out, static_cast<std::uint64_t>(value));
}

Result<void> put_string(std::vector<std::uint8_t>& out, const std::string& value, Limits limits) {
  if (value.size() > limits.max_string_bytes ||
      value.size() > std::numeric_limits<std::uint32_t>::max()) {
    return make_error(ErrorCode::ResourceLimit, "string exceeds OGR limit", "ogr");
  }
  put_u32(out, static_cast<std::uint32_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
  return {};
}

Result<void> put_value(std::vector<std::uint8_t>& out, const PropertyValue& value, Limits limits) {
  return std::visit(
      [&](const auto& item) -> Result<void> {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
          put_u8(out, 1);
          put_u8(out, item ? 1U : 0U);
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          put_u8(out, 2);
          put_i64(out, item);
        } else if constexpr (std::is_same_v<T, double>) {
          put_u8(out, 3);
          static_assert(sizeof(double) == sizeof(std::uint64_t));
          std::uint64_t bits = 0;
          std::memcpy(&bits, &item, sizeof(bits));
          put_u64(out, bits);
        } else {
          put_u8(out, 4);
          auto written = put_string(out, item, limits);
          if (!written) {
            return written;
          }
        }
        return {};
      },
      value);
}

Result<void> put_properties(std::vector<std::uint8_t>& out, const PropertyMap& properties,
                            Limits limits) {
  if (properties.size() > limits.max_properties ||
      properties.size() > std::numeric_limits<std::uint32_t>::max()) {
    return make_error(ErrorCode::ResourceLimit, "too many properties", "ogr");
  }
  put_u32(out, static_cast<std::uint32_t>(properties.size()));
  for (const auto& [key, value] : properties) {
    auto key_result = put_string(out, key, limits);
    if (!key_result) {
      return key_result;
    }
    auto value_result = put_value(out, value, limits);
    if (!value_result) {
      return value_result;
    }
  }
  return {};
}

Result<std::uint8_t> take_u8(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
  if (offset + 1U > bytes.size()) {
    return make_error(ErrorCode::Format, "unexpected end of payload", "ogr");
  }
  return bytes[offset++];
}

Result<std::uint16_t> take_u16(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
  if (offset + 2U > bytes.size()) {
    return make_error(ErrorCode::Format, "unexpected end of payload", "ogr");
  }
  const auto value = static_cast<std::uint16_t>(bytes[offset]) |
                     static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U])
                                                << 8U);
  offset += 2U;
  return value;
}

Result<std::uint32_t> take_u32(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
  if (offset + 4U > bytes.size()) {
    return make_error(ErrorCode::Format, "unexpected end of payload", "ogr");
  }
  std::uint32_t value = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
  }
  return value;
}

Result<std::uint64_t> take_u64(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
  if (offset + 8U > bytes.size()) {
    return make_error(ErrorCode::Format, "unexpected end of payload", "ogr");
  }
  std::uint64_t value = 0;
  for (int shift = 0; shift < 64; shift += 8) {
    value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
  }
  return value;
}

Result<std::int64_t> take_i64(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
  auto value = take_u64(bytes, offset);
  if (!value) {
    return value.error();
  }
  return static_cast<std::int64_t>(value.value());
}

Result<std::string> take_string(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
                                Limits limits) {
  auto size_result = take_u32(bytes, offset);
  if (!size_result) {
    return size_result.error();
  }
  const auto size = static_cast<std::size_t>(size_result.value());
  if (size > limits.max_string_bytes || offset + size > bytes.size()) {
    return make_error(ErrorCode::Format, "invalid string length", "ogr");
  }
  std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                    bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
  offset += size;
  return value;
}

Result<PropertyValue> take_value(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
                                 Limits limits) {
  auto tag = take_u8(bytes, offset);
  if (!tag) {
    return tag.error();
  }
  switch (tag.value()) {
    case 1: {
      auto value = take_u8(bytes, offset);
      if (!value) {
        return value.error();
      }
      return PropertyValue{value.value() != 0};
    }
    case 2: {
      auto value = take_i64(bytes, offset);
      if (!value) {
        return value.error();
      }
      return PropertyValue{value.value()};
    }
    case 3: {
      auto bits = take_u64(bytes, offset);
      if (!bits) {
        return bits.error();
      }
      double value = 0.0;
      const auto raw = bits.value();
      std::memcpy(&value, &raw, sizeof(value));
      if (!std::isfinite(value)) {
        return make_error(ErrorCode::Format, "non-finite double property", "ogr");
      }
      return PropertyValue{value};
    }
    case 4: {
      auto value = take_string(bytes, offset, limits);
      if (!value) {
        return value.error();
      }
      return PropertyValue{value.value()};
    }
    default:
      return make_error(ErrorCode::Unsupported, "unknown property value tag", "ogr");
  }
}

Result<PropertyMap> take_properties(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
                                    Limits limits) {
  auto count_result = take_u32(bytes, offset);
  if (!count_result) {
    return count_result.error();
  }
  if (count_result.value() > limits.max_properties) {
    return make_error(ErrorCode::ResourceLimit, "too many properties", "ogr");
  }
  PropertyMap properties;
  for (std::uint32_t i = 0; i < count_result.value(); ++i) {
    auto key = take_string(bytes, offset, limits);
    if (!key) {
      return key.error();
    }
    auto value = take_value(bytes, offset, limits);
    if (!value) {
      return value.error();
    }
    const auto inserted = properties.emplace(key.value(), value.value()).second;
    if (!inserted) {
      return make_error(ErrorCode::Format, "duplicate property key", "ogr");
    }
  }
  return properties;
}

std::vector<std::uint8_t> encode_header(std::uint64_t generation, CommitSeq latest,
                                        std::uint64_t record_count) {
  std::vector<std::uint8_t> header;
  header.reserve(kHeaderBytes);
  header.insert(header.end(), kMagic.begin(), kMagic.end());
  put_u16(header, kMajor);
  put_u16(header, kMinor);
  put_u32(header, static_cast<std::uint32_t>(kHeaderBytes));
  put_u64(header, generation);
  put_u64(header, latest.value);
  put_u64(header, record_count);
  const auto checksum = crc32c(header);
  put_u32(header, checksum);
  header.resize(kHeaderBytes, 0);
  return header;
}

Result<void> validate_header(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < kHeaderBytes) {
    return make_error(ErrorCode::Format, "store is smaller than OGR superblock", "ogr");
  }
  if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    return make_error(ErrorCode::Format, "bad OGR magic", "ogr");
  }
  std::size_t offset = 4;
  auto major = take_u16(bytes, offset);
  auto minor = take_u16(bytes, offset);
  auto header_bytes = take_u32(bytes, offset);
  if (!major || !minor || !header_bytes) {
    return make_error(ErrorCode::Format, "truncated OGR header", "ogr");
  }
  if (major.value() != kMajor || header_bytes.value() != kHeaderBytes) {
    return make_error(ErrorCode::Unsupported, "unsupported OGR header version", "ogr");
  }
  return {};
}

std::vector<std::uint8_t> encode_node(const NodeVersionView& node, bool tombstone,
                                      Limits limits) {
  std::vector<std::uint8_t> payload;
  put_u64(payload, node.id.value);
  put_i64(payload, node.interval.start);
  put_i64(payload, node.interval.end);
  put_u8(payload, tombstone ? 1U : 0U);
  (void)put_string(payload, node.label, limits);
  (void)put_properties(payload, node.properties, limits);
  return payload;
}

std::vector<std::uint8_t> encode_edge(const EdgeVersionView& edge, bool tombstone,
                                      Limits limits) {
  std::vector<std::uint8_t> payload;
  put_u64(payload, edge.id.value);
  put_u64(payload, edge.from.value);
  put_u64(payload, edge.to.value);
  put_i64(payload, edge.interval.start);
  put_i64(payload, edge.interval.end);
  put_u8(payload, tombstone ? 1U : 0U);
  (void)put_string(payload, edge.type, limits);
  (void)put_properties(payload, edge.properties, limits);
  return payload;
}

std::vector<std::uint8_t> encode_record(RecordType type, std::uint64_t logical_id,
                                        CommitSeq commit,
                                        const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> header;
  put_u16(header, static_cast<std::uint16_t>(type));
  put_u16(header, kSchema);
  put_u32(header, 0);
  put_u64(header, static_cast<std::uint64_t>(payload.size()));
  put_u64(header, logical_id);
  put_u64(header, commit.value);
  const auto header_crc = crc32c(header);
  const auto payload_crc = crc32c(payload);
  put_u32(header, header_crc);
  put_u32(header, payload_crc);
  std::vector<std::uint8_t> record = header;
  record.insert(record.end(), payload.begin(), payload.end());
  while (record.size() % 8U != 0U) {
    record.push_back(0);
  }
  return record;
}

struct DecodedRecord {
  RecordType type;
  std::uint64_t logical_id{0};
  CommitSeq commit{};
  std::vector<std::uint8_t> payload;
  std::size_t next_offset{0};
};

Result<DecodedRecord> decode_record(const std::vector<std::uint8_t>& bytes, std::size_t offset,
                                    Limits limits) {
  if (offset + kRecordHeaderBytes > bytes.size()) {
    return make_error(ErrorCode::Format, "truncated record header", "ogr");
  }
  const auto header_begin = offset;
  auto type_raw = take_u16(bytes, offset);
  auto schema = take_u16(bytes, offset);
  auto flags = take_u32(bytes, offset);
  auto payload_len = take_u64(bytes, offset);
  auto logical_id = take_u64(bytes, offset);
  auto commit_seq = take_u64(bytes, offset);
  auto header_crc = take_u32(bytes, offset);
  auto payload_crc = take_u32(bytes, offset);
  if (!type_raw || !schema || !flags || !payload_len || !logical_id || !commit_seq ||
      !header_crc || !payload_crc) {
    return make_error(ErrorCode::Format, "truncated record", "ogr");
  }
  if (schema.value() != kSchema || flags.value() != 0) {
    return make_error(ErrorCode::Unsupported, "unsupported record schema or flags", "ogr");
  }
  if (payload_len.value() > limits.max_record_bytes) {
    return make_error(ErrorCode::ResourceLimit, "record payload exceeds configured limit", "ogr");
  }
  std::vector<std::uint8_t> header_for_crc(bytes.begin() + static_cast<std::ptrdiff_t>(header_begin),
                                           bytes.begin() + static_cast<std::ptrdiff_t>(header_begin + 32U));
  if (crc32c(header_for_crc) != header_crc.value()) {
    return make_error(ErrorCode::Integrity, "record header checksum mismatch", "ogr");
  }
  const auto payload_size = static_cast<std::size_t>(payload_len.value());
  if (offset + payload_size > bytes.size()) {
    return make_error(ErrorCode::Format, "truncated record payload", "ogr");
  }
  std::vector<std::uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
  if (crc32c(payload) != payload_crc.value()) {
    return make_error(ErrorCode::Integrity, "record payload checksum mismatch", "ogr");
  }
  offset += payload_size;
  while (offset % 8U != 0U && offset < bytes.size()) {
    if (bytes[offset] != 0) {
      return make_error(ErrorCode::Format, "non-zero record padding", "ogr");
    }
    ++offset;
  }
  auto type = static_cast<RecordType>(type_raw.value());
  switch (type) {
    case RecordType::TxnBegin:
    case RecordType::NodeVersion:
    case RecordType::EdgeVersion:
    case RecordType::TxnCommit:
      break;
    default:
      return make_error(ErrorCode::Unsupported, "unknown required OGR record", "ogr");
  }
  return DecodedRecord{type, logical_id.value(), CommitSeq{commit_seq.value()}, std::move(payload),
                       offset};
}

Result<NodeVersionView> decode_node(const std::vector<std::uint8_t>& payload, CommitSeq commit,
                                    bool& tombstone, Limits limits) {
  std::size_t offset = 0;
  auto id = take_u64(payload, offset);
  auto start = take_i64(payload, offset);
  auto end = take_i64(payload, offset);
  auto tombstone_raw = take_u8(payload, offset);
  if (!id || !start || !end || !tombstone_raw) {
    return make_error(ErrorCode::Format, "truncated node record", "ogr");
  }
  auto label = take_string(payload, offset, limits);
  auto properties = take_properties(payload, offset, limits);
  if (!label || !properties) {
    return !label ? label.error() : properties.error();
  }
  if (offset != payload.size()) {
    return make_error(ErrorCode::Format, "trailing bytes in node record", "ogr");
  }
  tombstone = tombstone_raw.value() != 0;
  NodeVersionView node{NodeId{id.value()}, label.value(), Interval{start.value(), end.value()},
                       properties.value(), commit};
  if (!tombstone) {
    auto interval_result = validate_interval(node.interval);
    if (!interval_result) {
      return interval_result.error();
    }
  }
  return node;
}

Result<EdgeVersionView> decode_edge(const std::vector<std::uint8_t>& payload, CommitSeq commit,
                                    bool& tombstone, Limits limits) {
  std::size_t offset = 0;
  auto id = take_u64(payload, offset);
  auto from = take_u64(payload, offset);
  auto to = take_u64(payload, offset);
  auto start = take_i64(payload, offset);
  auto end = take_i64(payload, offset);
  auto tombstone_raw = take_u8(payload, offset);
  if (!id || !from || !to || !start || !end || !tombstone_raw) {
    return make_error(ErrorCode::Format, "truncated edge record", "ogr");
  }
  auto type = take_string(payload, offset, limits);
  auto properties = take_properties(payload, offset, limits);
  if (!type || !properties) {
    return !type ? type.error() : properties.error();
  }
  if (offset != payload.size()) {
    return make_error(ErrorCode::Format, "trailing bytes in edge record", "ogr");
  }
  tombstone = tombstone_raw.value() != 0;
  EdgeVersionView edge{EdgeId{id.value()}, NodeId{from.value()}, NodeId{to.value()},
                       type.value(), Interval{start.value(), end.value()}, properties.value(),
                       commit};
  if (!tombstone) {
    auto interval_result = validate_interval(edge.interval);
    if (!interval_result) {
      return interval_result.error();
    }
  }
  return edge;
}

Result<std::vector<std::uint8_t>> read_all(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return make_error(ErrorCode::Io, "failed to open OGR store for reading", "ogr");
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                   std::istreambuf_iterator<char>());
}

}  // namespace

std::uint32_t crc32c(const std::vector<std::uint8_t>& bytes) noexcept {
  std::uint32_t crc = 0xffffffffU;
  for (auto byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      const auto mask = static_cast<std::uint32_t>(-(crc & 1U));
      crc = (crc >> 1U) ^ (0x82f63b78U & mask);
    }
  }
  return ~crc;
}

Result<void> create_empty(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return make_error(ErrorCode::Io, "failed to create OGR store", "ogr");
  }
  const auto header = encode_header(1, CommitSeq{0}, 0);
  out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
  if (!out) {
    return make_error(ErrorCode::Io, "failed to write OGR header", "ogr");
  }
  return {};
}

Result<StoreImage> read_store(const std::filesystem::path& path, Limits limits) {
  auto bytes_result = read_all(path);
  if (!bytes_result) {
    return bytes_result.error();
  }
  const auto& bytes = bytes_result.value();
  auto header = validate_header(bytes);
  if (!header) {
    return header.error();
  }

  StoreImage image;
  image.generation = 1;
  std::size_t offset = kHeaderBytes;
  std::vector<DecodedRecord> group;
  while (offset < bytes.size()) {
    auto decoded = decode_record(bytes, offset, limits);
    if (!decoded) {
      if (decoded.error().code == ErrorCode::Format) {
        break;
      }
      return decoded.error();
    }
    offset = decoded.value().next_offset;
    if (decoded.value().type == RecordType::TxnBegin) {
      group.clear();
      group.push_back(std::move(decoded.value()));
      continue;
    }
    if (group.empty()) {
      return make_error(ErrorCode::Format, "record outside transaction group", "ogr");
    }
    group.push_back(std::move(decoded.value()));
    if (group.back().type != RecordType::TxnCommit) {
      continue;
    }

    const CommitSeq commit = group.back().commit;
    std::size_t commit_payload_offset = 0;
    auto commit_payload_commit = take_u64(group.back().payload, commit_payload_offset);
    auto declared_count = take_u64(group.back().payload, commit_payload_offset);
    auto group_crc = take_u32(group.back().payload, commit_payload_offset);
    if (!commit_payload_commit || !declared_count || !group_crc ||
        commit_payload_offset != group.back().payload.size()) {
      return make_error(ErrorCode::Format, "bad TXN_COMMIT payload", "ogr");
    }
    if (commit_payload_commit.value() != commit.value ||
        declared_count.value() != static_cast<std::uint64_t>(group.size())) {
      return make_error(ErrorCode::Integrity, "transaction group count mismatch", "ogr");
    }
    std::vector<std::uint8_t> digest_input;
    for (std::size_t i = 0; i + 1U < group.size(); ++i) {
      digest_input.insert(digest_input.end(), group[i].payload.begin(), group[i].payload.end());
    }
    if (crc32c(digest_input) != group_crc.value()) {
      return make_error(ErrorCode::Integrity, "transaction group digest mismatch", "ogr");
    }

    for (std::size_t i = 1; i + 1U < group.size(); ++i) {
      if (group[i].type == RecordType::NodeVersion) {
        bool tombstone = false;
        auto node = decode_node(group[i].payload, commit, tombstone, limits);
        if (!node) {
          return node.error();
        }
        if (tombstone) {
          image.node_tombstones.push_back(node.value());
        } else {
          image.node_versions.push_back(node.value());
        }
      } else if (group[i].type == RecordType::EdgeVersion) {
        bool tombstone = false;
        auto edge = decode_edge(group[i].payload, commit, tombstone, limits);
        if (!edge) {
          return edge.error();
        }
        if (tombstone) {
          image.edge_tombstones.push_back(edge.value());
        } else {
          image.edge_versions.push_back(edge.value());
        }
      }
    }
    image.latest_commit = commit;
    group.clear();
  }
  return image;
}

Result<void> append_transaction(const std::filesystem::path& path, CommitSeq parent,
                                CommitSeq commit, const std::vector<NodeVersionView>& nodes,
                                const std::vector<EdgeVersionView>& edges,
                                const std::vector<NodeId>& node_tombstones,
                                const std::vector<EdgeId>& edge_tombstones, Limits limits) {
  std::vector<std::vector<std::uint8_t>> records;
  std::vector<std::uint8_t> begin_payload;
  put_u64(begin_payload, parent.value);
  put_u64(begin_payload,
          static_cast<std::uint64_t>(nodes.size() + edges.size() + node_tombstones.size() +
                                     edge_tombstones.size()));
  records.push_back(encode_record(RecordType::TxnBegin, commit.value, commit, begin_payload));

  std::vector<std::uint8_t> digest_input;
  digest_input.insert(digest_input.end(), begin_payload.begin(), begin_payload.end());

  for (const auto& node : nodes) {
    auto payload = encode_node(node, false, limits);
    digest_input.insert(digest_input.end(), payload.begin(), payload.end());
    records.push_back(encode_record(RecordType::NodeVersion, node.id.value, commit, payload));
  }
  for (const auto& id : node_tombstones) {
    NodeVersionView tombstone{id, "", Interval{0, 1}, {}, commit};
    auto payload = encode_node(tombstone, true, limits);
    digest_input.insert(digest_input.end(), payload.begin(), payload.end());
    records.push_back(encode_record(RecordType::NodeVersion, id.value, commit, payload));
  }
  for (const auto& edge : edges) {
    auto payload = encode_edge(edge, false, limits);
    digest_input.insert(digest_input.end(), payload.begin(), payload.end());
    records.push_back(encode_record(RecordType::EdgeVersion, edge.id.value, commit, payload));
  }
  for (const auto& id : edge_tombstones) {
    EdgeVersionView tombstone{id, NodeId{0}, NodeId{0}, "", Interval{0, 1}, {}, commit};
    auto payload = encode_edge(tombstone, true, limits);
    digest_input.insert(digest_input.end(), payload.begin(), payload.end());
    records.push_back(encode_record(RecordType::EdgeVersion, id.value, commit, payload));
  }

  std::vector<std::uint8_t> commit_payload;
  put_u64(commit_payload, commit.value);
  put_u64(commit_payload, static_cast<std::uint64_t>(records.size() + 1U));
  put_u32(commit_payload, crc32c(digest_input));
  records.push_back(encode_record(RecordType::TxnCommit, commit.value, commit, commit_payload));

  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    return make_error(ErrorCode::Io, "failed to open OGR store for append", "ogr");
  }
  for (const auto& record : records) {
    out.write(reinterpret_cast<const char*>(record.data()),
              static_cast<std::streamsize>(record.size()));
    if (!out) {
      return make_error(ErrorCode::Io, "failed to append OGR record", "ogr");
    }
  }
  out.close();
  if (!out) {
    return make_error(ErrorCode::Io, "failed to flush OGR transaction", "ogr");
  }

  auto current = read_store(path, limits);
  if (!current) {
    return current.error();
  }
  auto header = encode_header(1, current.value().latest_commit,
                              static_cast<std::uint64_t>(records.size()));
  std::fstream patch(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!patch) {
    return make_error(ErrorCode::Io, "failed to update OGR header", "ogr");
  }
  patch.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
  if (!patch) {
    return make_error(ErrorCode::Io, "failed to write OGR header update", "ogr");
  }
  return {};
}

}  // namespace orbit::ogr
