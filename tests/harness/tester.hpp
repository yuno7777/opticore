// Minimal deterministic test harness for OptiCore (no third-party deps).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace oc_test {

inline std::vector<std::pair<std::string, std::function<bool()>>>& tests() {
  static std::vector<std::pair<std::string, std::function<bool()>>> t;
  return t;
}

struct Registrar {
  Registrar(const char* name, bool (*fn)()) {
    tests().emplace_back(name, fn);
  }
};

inline const char* current() {
  static std::string cur;
  return cur.c_str();
}
inline void set_current(const std::string& name) {
  static std::string cur;
  cur = name;
}

inline int run_all(const char* argv0) {
  (void)argv0;
  int failures = 0;
  for (const auto& t : tests()) {
    std::fprintf(stdout, "RUN   %s\n", t.first.c_str());
    const bool ok = t.second();
    if (ok) {
      std::fprintf(stdout, "PASS  %s\n", t.first.c_str());
    } else {
      ++failures;
      std::fprintf(stdout, "FAIL  %s\n", t.first.c_str());
    }
  }
  std::fprintf(stdout, "tests: %zu run, %d failed\n", tests().size(), failures);
  return failures == 0 ? 0 : 1;
}

}  // namespace oc_test

#define OC_TEST(fn_name, label)                               \
  static bool fn_name();                                      \
  static oc_test::Registrar fn_name##_reg(label, fn_name);    \
  static bool fn_name()

#define OC_CHECK(cond, msg)                                     \
  do {                                                          \
    if (!(cond)) {                                              \
      std::fprintf(stderr, "  CHECK FAILED: %s\n", msg);        \
      std::fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
      return false;                                             \
    }                                                           \
  } while (0)

#define OC_CHECK_NEAR(a, b, tol, msg)                                    \
  do {                                                                   \
    const double _d = (a) - (b);                                         \
    if (!(_d <= (tol) && _d >= -(tol))) {                                \
      std::fprintf(stderr, "  CHECK FAILED (near): %s (%.9f vs %.9f)\n", \
                   msg, (double)(a), (double)(b));                       \
      std::fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__);          \
      return false;                                                      \
    }                                                                    \
  } while (0)
