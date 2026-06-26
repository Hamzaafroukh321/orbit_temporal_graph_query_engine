#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace orbit::test {

using TestFn = void (*)();
void register_test(const char* name, TestFn fn);

struct Registrar {
  Registrar(const char* name, TestFn fn) { register_test(name, fn); }
};

inline void require(bool condition, const char* expression, const char* file, int line) {
  if (!condition) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             " requirement failed: " + expression);
  }
}

inline std::filesystem::path temp_path(const std::string& name) {
  auto path = std::filesystem::temp_directory_path() / ("orbit_" + name);
  std::filesystem::remove(path);
  return path;
}

}  // namespace orbit::test

#define ORBIT_TEST(name)                                      \
  void name();                                                \
  namespace {                                                 \
  ::orbit::test::Registrar registrar_##name(#name, &name);    \
  }                                                           \
  void name()

#define REQUIRE(expr) ::orbit::test::require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
