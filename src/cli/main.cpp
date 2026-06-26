#include "orbit/store.hpp"

#include <filesystem>
#include <fstream>
#include <cctype>
#include <iostream>
#include <sstream>

namespace {

int print_error(const orbit::Error& error) {
  std::cerr << error.describe() << "\n";
  switch (error.code) {
    case orbit::ErrorCode::Usage:
    case orbit::ErrorCode::QuerySyntax:
    case orbit::ErrorCode::QueryType:
      return 2;
    case orbit::ErrorCode::ResourceLimit:
      return 4;
    case orbit::ErrorCode::Cancelled:
      return 5;
    case orbit::ErrorCode::InternalInvariant:
      return 10;
    default:
      return 3;
  }
}

orbit::PropertyValue parse_value(const std::string& text) {
  if (text == "true") {
    return true;
  }
  if (text == "false") {
    return false;
  }
  bool numeric = !text.empty();
  std::size_t start = text.starts_with('-') ? 1U : 0U;
  for (std::size_t i = start; i < text.size(); ++i) {
    numeric = numeric && std::isdigit(static_cast<unsigned char>(text[i])) != 0;
  }
  if (numeric && start < text.size()) {
    return static_cast<std::int64_t>(std::stoll(text));
  }
  return text;
}

orbit::PropertyMap parse_properties(std::istringstream& line) {
  orbit::PropertyMap properties;
  std::string token;
  while (line >> token) {
    const auto eq = token.find('=');
    if (eq == std::string::npos || eq == 0) {
      continue;
    }
    properties.emplace(token.substr(0, eq), parse_value(token.substr(eq + 1U)));
  }
  return properties;
}

orbit::Result<void> apply_script(orbit::GraphStore& store, const std::filesystem::path& script) {
  std::ifstream input(script);
  if (!input) {
    return orbit::make_error(orbit::ErrorCode::Io, "failed to open mutation script", "cli");
  }
  auto txn = store.begin();
  if (!txn) {
    return txn.error();
  }
  std::string line_text;
  std::size_t line_number = 0;
  while (std::getline(input, line_text)) {
    ++line_number;
    if (line_text.empty() || line_text[0] == '#') {
      continue;
    }
    std::istringstream line(line_text);
    std::string kind;
    line >> kind;
    if (kind == "node") {
      std::uint64_t id = 0;
      std::string label;
      std::int64_t start = 0;
      std::int64_t end = 0;
      line >> id >> label >> start >> end;
      if (!line) {
        return orbit::make_error(orbit::ErrorCode::Usage,
                                 "bad node mutation at line " + std::to_string(line_number),
                                 "cli");
      }
      auto result = txn.value().put_node(orbit::NodeId{id}, label, orbit::Interval{start, end},
                                         parse_properties(line));
      if (!result) {
        return result;
      }
    } else if (kind == "edge") {
      std::uint64_t id = 0;
      std::uint64_t from = 0;
      std::uint64_t to = 0;
      std::string type;
      std::int64_t start = 0;
      std::int64_t end = 0;
      line >> id >> from >> to >> type >> start >> end;
      if (!line) {
        return orbit::make_error(orbit::ErrorCode::Usage,
                                 "bad edge mutation at line " + std::to_string(line_number),
                                 "cli");
      }
      auto result = txn.value().put_edge(orbit::EdgeId{id}, orbit::NodeId{from}, orbit::NodeId{to},
                                         type, orbit::Interval{start, end}, parse_properties(line));
      if (!result) {
        return result;
      }
    } else if (kind == "delete_node") {
      std::uint64_t id = 0;
      line >> id;
      auto result = txn.value().delete_node(orbit::NodeId{id});
      if (!result) {
        return result;
      }
    } else if (kind == "delete_edge") {
      std::uint64_t id = 0;
      line >> id;
      auto result = txn.value().delete_edge(orbit::EdgeId{id});
      if (!result) {
        return result;
      }
    } else {
      return orbit::make_error(orbit::ErrorCode::Usage,
                               "unknown mutation kind at line " + std::to_string(line_number),
                               "cli");
    }
  }
  auto commit = txn.value().commit();
  if (!commit) {
    return commit.error();
  }
  std::cout << "commit " << commit.value().value << "\n";
  return {};
}

std::int64_t extract_time(int argc, char** argv, int start_index) {
  for (int i = start_index; i + 1 < argc; ++i) {
    if (std::string_view(argv[i]) == "--time") {
      return std::stoll(argv[i + 1]);
    }
  }
  std::string all;
  for (int i = start_index; i < argc; ++i) {
    all += argv[i];
    all.push_back(' ');
  }
  const auto time_pos = all.find("TIME ");
  if (time_pos == std::string::npos) {
    return 0;
  }
  std::istringstream input(all.substr(time_pos + 5U));
  std::int64_t value = 0;
  input >> value;
  return value;
}

std::string collect_query(int argc, char** argv, int start_index) {
  std::string query;
  for (int i = start_index; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--time") {
      ++i;
      continue;
    }
    if (!query.empty()) {
      query.push_back(' ');
    }
    query += argv[i];
  }
  return query;
}

void print_batch(const orbit::ResultBatch& batch) {
  for (const auto& row : batch.rows) {
    for (std::size_t i = 0; i < row.values.size(); ++i) {
      if (i != 0) {
        std::cout << '\t';
      }
      std::cout << orbit::canonical_value(row.values[i]);
    }
    std::cout << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: orbit <init|apply|query|explain|check|inspect> ...\n";
    return 2;
  }

  const std::string command = argv[1];
  if (command == "init") {
    if (argc != 3) {
      std::cerr << "usage: orbit init <store.ogr>\n";
      return 2;
    }
    auto store = orbit::GraphStore::open(argv[2], orbit::OpenOptions{true});
    if (!store) {
      return print_error(store.error());
    }
    std::cout << "initialized " << argv[2] << "\n";
    return 0;
  }

  if (argc < 3) {
    std::cerr << "usage: orbit " << command << " <store.ogr> ...\n";
    return 2;
  }

  auto store = orbit::GraphStore::open(argv[2]);
  if (!store) {
    return print_error(store.error());
  }

  if (command == "apply") {
    if (argc != 4) {
      std::cerr << "usage: orbit apply <store.ogr> <script.oms>\n";
      return 2;
    }
    auto applied = apply_script(store.value(), argv[3]);
    return applied ? 0 : print_error(applied.error());
  }
  if (command == "check") {
    auto checked = store.value().check();
    if (!checked) {
      return print_error(checked.error());
    }
    std::cout << "ok latest_commit=" << store.value().latest_commit().value << "\n";
    return 0;
  }
  if (command == "inspect") {
    std::cout << store.value().inspect();
    return 0;
  }
  if (command == "query" || command == "explain") {
    if (argc < 4) {
      std::cerr << "usage: orbit " << command << " <store.ogr> <query> [--time n]\n";
      return 2;
    }
    const auto valid_time = extract_time(argc, argv, 3);
    const auto query = collect_query(argc, argv, 3);
    auto prepared = store.value().prepare(query);
    if (!prepared) {
      return print_error(prepared.error());
    }
    if (command == "explain") {
      auto plan = prepared.value().explain();
      std::cout << "fingerprint " << plan.fingerprint << "\n";
      for (const auto& op : plan.operators) {
        std::cout << op << "\n";
      }
      return 0;
    }
    auto snapshot = store.value().snapshot(orbit::SnapshotSelector{std::nullopt, valid_time});
    if (!snapshot) {
      return print_error(snapshot.error());
    }
    auto cursor = prepared.value().execute(snapshot.value());
    if (!cursor) {
      return print_error(cursor.error());
    }
    while (true) {
      auto batch = cursor.value().next(1024);
      if (!batch) {
        return print_error(batch.error());
      }
      if (!batch.value()) {
        break;
      }
      print_batch(*batch.value());
    }
    return 0;
  }

  std::cerr << "unknown command: " << command << "\n";
  return 2;
}
