#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace orbit::test {

using TestFn = void (*)();

struct TestCase {
  const char* name;
  TestFn fn;
};

std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

void register_test(const char* name, TestFn fn) {
  registry().push_back(TestCase{name, fn});
}

}  // namespace orbit::test

int main() {
  int failed = 0;
  for (const auto& test : orbit::test::registry()) {
    try {
      test.fn();
      std::cout << "[PASS] " << test.name << "\n";
    } catch (const std::exception& ex) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
    } catch (...) {
      ++failed;
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
    }
  }
  std::cout << orbit::test::registry().size() << " tests, " << failed << " failed\n";
  return failed == 0 ? 0 : 1;
}
