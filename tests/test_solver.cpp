// Solver tests: determinism, quality sanity, unserved handling, regressions.
#include <cstdio>
#include <string>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/scenario.hpp"
#include "opticore/solution.hpp"
#include "opticore/validator.hpp"
#include "city_model.hpp"
#include "chronos.hpp"
#include "harness/tester.hpp"

namespace {

oc::CitySpec make_spec(uint64_t seed, int customers, int vehicles) {
  oc::CitySpec spec;
  spec.name = "solver-test-city";
  spec.seed = seed;
  spec.customers = customers;
  spec.clusters = 3;
  spec.vehicle_count = vehicles;
  spec.area_km = 20.0;
  spec.speed_kmh = 30.0;
  spec.horizon_min = 600.0;
  spec.time_window_mode = oc::TimeWindowMode::Hard;
  return spec;
}

OC_TEST(solver_determinism_two_runs, "solver: identical seed -> byte-identical solution") {
  const oc::Scenario sc = oc::generate_city_scenario(make_spec(77, 14, 4));
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 5;
  const oc::Solution a = solver.solve(sc, opts);
  const oc::Solution b = solver.solve(sc, opts);
  const std::string ta = oc::solution_to_text(a);
  const std::string tb = oc::solution_to_text(b);
  OC_CHECK(ta == tb, "two runs with same seed produce byte-identical files");
  return true;
}

OC_TEST(solver_validates_on_medium, "solver: medium instance passes validator") {
  const oc::Scenario sc = oc::generate_city_scenario(make_spec(2026, 18, 5));
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 11;
  const oc::Solution sol = solver.solve(sc, opts);
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  if (!vr.ok) {
    for (const auto& v : vr.violations)
      std::fprintf(stderr, "  VIOLATION %s: %s\n", v.code.c_str(),
                   v.message.c_str());
  }
  OC_CHECK(vr.ok, "solver output must pass independent validator");
  int served = 0;
  for (const auto& r : sol.routes) served += static_cast<int>(r.size());
  OC_CHECK(served > 0, "solver must serve some customers");
  if (served < 18) {
    OC_CHECK(static_cast<int>(sol.unserved.size()) + served == 18,
             "served + unserved must equal customer count");
  }
  // claimed objective must equal the recomputed one (validator guarantees)
  OC_CHECK_NEAR(sol.claimed.total_cost, vr.recomputed.total_cost, 1e-6,
                "claimed vs recomputed objective");
  return true;
}

OC_TEST(solver_local_search_improves, "solver: more ILS phases do not worsen the incumbent") {
  const oc::Scenario sc = oc::generate_city_scenario(make_spec(313, 20, 5));
  oc::ChronosSolver solver;
  oc::SolverOptions quick_opts;
  quick_opts.seed = 3;
  quick_opts.max_iterations = 1;  // construction + LS + a single phase
  const oc::Solution quick = solver.solve(sc, quick_opts);
  oc::SolverOptions long_opts;
  long_opts.seed = 3;
  long_opts.max_iterations = 200;
  const oc::Solution long_run = solver.solve(sc, long_opts);
  OC_CHECK(long_run.claimed.total_cost <= quick.claimed.total_cost + 1e-9,
           "more ILS phases must not worsen the incumbent");
  return true;
}

OC_TEST(solver_capacity_unserved_reason, "solver: capacity-infeasible customer listed unserved") {
  oc::CitySpec spec = make_spec(8, 6, 2);
  spec.capacity = 5;  // demand range is 2..12, some customers exceed 5
  spec.name = "tight-capacity";
  const oc::Scenario sc = oc::generate_city_scenario(spec);
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 9;
  const oc::Solution sol = solver.solve(sc, opts);
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  if (!vr.ok) {
    for (const auto& v : vr.violations)
      std::fprintf(stderr, "  VIOLATION %s: %s\n", v.code.c_str(),
                   v.message.c_str());
  }
  OC_CHECK(vr.ok, "capacity-tight scenario still validates");
  bool any_capacity_reason = false;
  for (const auto& u : sol.unserved) {
    if (u.second.find("capacity") != std::string::npos)
      any_capacity_reason = true;
  }
  OC_CHECK(any_capacity_reason,
           "at least one unserved reason mentions capacity");
  return true;
}

OC_TEST(solver_soft_tw_penalized, "solver: soft TW mode produces valid penalized solution") {
  oc::CitySpec spec = make_spec(45, 10, 3);
  spec.time_window_mode = oc::TimeWindowMode::Soft;
  spec.name = "soft-tw";
  const oc::Scenario sc = oc::generate_city_scenario(spec);
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 2;
  const oc::Solution sol = solver.solve(sc, opts);
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  if (!vr.ok) {
    for (const auto& v : vr.violations)
      std::fprintf(stderr, "  VIOLATION %s: %s\n", v.code.c_str(),
                   v.message.c_str());
  }
  OC_CHECK(vr.ok, "soft TW solution must still validate");
  return true;
}

OC_TEST(solver_timeout_flag, "solver: tiny budget yields timeout or feasible, never invalid") {
  const oc::Scenario sc = oc::generate_city_scenario(make_spec(11, 16, 4));
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 4;
  opts.time_budget_s = 0.05;
  const oc::Solution sol = solver.solve(sc, opts);
  OC_CHECK(sol.status != oc::SolverStatus::Failed, "no internal failure");
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  OC_CHECK(vr.ok, "budgeted run must still emit a valid incumbent");
  return true;
}

}  // namespace

int main() { return oc_test::run_all(nullptr); }
