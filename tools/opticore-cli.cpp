// OptiCore command-line interface.
//
// Subcommands:
//   opticore gen --out FILE ...
//   opticore solve --scenario FILE --solution FILE [--solver chronos-cpu]
//   opticore validate --scenario FILE --solution FILE
//   opticore list-solvers
//   opticore info
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/scenario.hpp"
#include "opticore/solution.hpp"
#include "opticore/validator.hpp"
#include "opticore/version.hpp"
#include "city_model.hpp"
#include "chronos.hpp"

namespace {

using Args = std::map<std::string, std::string>;
using Flags = std::vector<std::string>;

const std::string kUsage =
    "opticore - GPU-era urban logistics optimization engine (CPU edition)\n"
    "\n"
    "Usage:\n"
    "  opticore gen --out FILE [--seed N] [--customers N] [--clusters N]\n"
    "      [--vehicles N] [--capacity Q] [--area KM] [--speed KMH]\n"
    "      [--horizon MIN] [--duration-limit MIN(=none)] [--tw-mode hard|soft]\n"
    "      [--name S]\n"
    "  opticore solve --scenario FILE --solution FILE [--solver chronos-cpu]\n"
    "      [--seed N] [--time-budget-s S] [--max-iterations N]\n"
    "  opticore validate --scenario FILE --solution FILE\n"
    "  opticore list-solvers\n"
    "  opticore info\n";

std::string get_str(const Flags& flags, const std::string& name,
                    std::string def) {
  for (size_t i = 0; i + 1 < flags.size(); ++i) {
    if ((flags[i] == "--" + name) && !flags[i + 1].empty() &&
        flags[i + 1][0] != '-')
      return flags[i + 1];
  }
  return def;
}

long long get_integer(const Flags& flags, const char* name, long long def) {
  const std::string v = get_str(flags, name, "");
  if (v.empty()) return def;
  try {
    return std::stoll(v);
  } catch (...) {
    std::fprintf(stderr, "bad integer for --%s: %s\n", name, v.c_str());
    std::exit(1);
  }
}

double get_double(const Flags& flags, const char* name, double def) {
  const std::string v = get_str(flags, name, "");
  if (v.empty()) return def;
  try {
    return std::stod(v);
  } catch (...) {
    std::fprintf(stderr, "bad number for --%s: %s\n", name, v.c_str());
    std::exit(1);
  }
}

int cmd_gen(const Flags& flags) {
  const std::string out = get_str(flags, "out", "");
  if (out.empty()) {
    std::fprintf(stderr, "gen: --out is required\n%s", kUsage.c_str());
    return 1;
  }
  oc::CitySpec spec;
  spec.name = get_str(flags, "name", "city-scenario");
  spec.seed = static_cast<uint64_t>(get_integer(flags, "seed", 1));
  if (spec.seed < 0) spec.seed = 0;
  spec.customers = static_cast<int>(get_integer(flags, "customers", 16));
  spec.clusters = static_cast<int>(get_integer(flags, "clusters", 3));
  spec.vehicle_count = static_cast<int>(get_integer(flags, "vehicles", 8));
  spec.capacity = static_cast<int>(get_integer(flags, "capacity", 0));
  spec.area_km = get_double(flags, "area", 20.0);
  spec.speed_kmh = get_double(flags, "speed", 30.0);
  spec.horizon_min = get_double(flags, "horizon", 600.0);
  spec.max_route_duration = get_double(flags, "duration-limit", -1.0);
  const std::string tw = get_str(flags, "tw-mode", "hard");
  if (!oc::time_window_mode_from_string(tw, &spec.time_window_mode)) {
    std::fprintf(stderr, "gen: bad --tw-mode '%s'\n", tw.c_str());
    return 1;
  }

  oc::Scenario sc = oc::generate_city_scenario(spec);
  const std::vector<std::string> problems = oc::scenario_sanity(sc);
  if (!problems.empty()) {
    std::fprintf(stderr, "gen: generated scenario is not well-formed:\n");
    for (const auto& p : problems) std::fprintf(stderr, "  - %s\n", p.c_str());
    return 1;
  }
  const std::string text = oc::scenario_to_text(sc);
  if (get_str(flags, "echo-text", "") == "1") std::cout << text;
  std::FILE* f = std::fopen(out.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "gen: cannot open %s for writing\n", out.c_str());
    return 1;
  }
  std::fwrite(text.data(), 1, text.size(), f);
  std::fclose(f);

  int n_customers = 0;
  for (const auto& nd : sc.nodes)
    if (nd.demand > 0) ++n_customers;
  std::printf(
      "gen: wrote %s (nodes=%d customers=%d vehicles=%d capacity=%d "
      "tw_mode=%s seed=%llu)\n",
      out.c_str(), sc.node_count(), n_customers, sc.vehicle_count, sc.capacity,
      oc::time_window_mode_to_string(sc.time_window_mode),
      static_cast<unsigned long long>(sc.seed));
  return 0;
}

int cmd_solve(const Flags& flags) {
  const std::string scenario_path = get_str(flags, "scenario", "");
  const std::string solution_path = get_str(flags, "solution", "");
  const std::string solver_name = get_str(flags, "solver", "chronos-cpu");
  const int verbosity = static_cast<int>(get_integer(flags, "verbosity", 0));

  if (scenario_path.empty() || solution_path.empty()) {
    std::fprintf(stderr, "solve: --scenario and --solution are required\n");
    return 1;
  }
  if (solver_name != "chronos-cpu") {
    std::fprintf(stderr, "solve: unknown solver '%s' (available: chronos-cpu)\n",
                 solver_name.c_str());
    return 1;
  }

  oc::Scenario sc = oc::scenario_from_file(scenario_path);
  const auto problems = oc::scenario_sanity(sc);
  if (!problems.empty()) {
    std::fprintf(stderr, "solve: scenario failed sanity checks:\n");
    for (const auto& p : problems) std::fprintf(stderr, "  - %s\n", p.c_str());
    return 1;
  }

  oc::SolverOptions opts;
  const std::string seed_s = get_str(flags, "seed", "");
  opts.seed = seed_s.empty() ? sc.seed
                             : static_cast<uint64_t>(get_integer(flags, "seed", 1));
  if (opts.seed < 0) opts.seed = 0;
  opts.time_budget_s = get_double(flags, "time-budget-s", 0.0);
  opts.max_iterations = get_integer(flags, "max-iterations", 0);
  if (opts.time_budget_s < 0) opts.time_budget_s = 0;
  if (opts.max_iterations < 0) opts.max_iterations = 0;
  opts.verbosity = verbosity;

  oc::ChronosSolver solver;
  oc::Solution sol = solver.solve(sc, opts);

  // The solver must never hand back an invalid solution (belt-and-braces).
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  if (!vr.ok) {
    std::fprintf(stderr, "solve: internal error - solver emitted an invalid "
                         "solution:\n");
    for (const auto& v : vr.violations)
      std::fprintf(stderr, "  VIOLATION %s: %s\n", v.code.c_str(),
                   v.message.c_str());
    return 3;
  }

  const std::string text = oc::solution_to_text(sol);
  std::FILE* f = std::fopen(solution_path.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "solve: cannot open %s for writing\n",
                 solution_path.c_str());
    return 1;
  }
  std::fwrite(text.data(), 1, text.size(), f);
  std::fclose(f);

  int served = 0;
  for (const auto& r : sol.routes) served += static_cast<int>(r.size());
  std::printf(
      "solve: solver=%s status=%s total_cost=%.6f travel_time_min=%.3f "
      "travel_dist_km=%.3f vehicles_used=%d lateness_min=%.3f served=%d "
      "unserved=%d\n",
      sol.solver.c_str(), oc::solver_status_to_string(sol.status),
      sol.claimed.total_cost, sol.claimed.travel_time_min,
      sol.claimed.travel_dist_km, sol.claimed.vehicles_used,
      sol.claimed.total_lateness_min, served, sol.claimed.unserved_count);
  return 0;
}

int cmd_validate(const Flags& flags) {
  const std::string scenario_path = get_str(flags, "scenario", "");
  const std::string solution_path = get_str(flags, "solution", "");
  if (scenario_path.empty() || solution_path.empty()) {
    std::fprintf(stderr, "validate: --scenario and --solution are required\n");
    return 1;
  }
  oc::Solution sol = oc::solution_from_file(solution_path);
  oc::Scenario sc = oc::scenario_from_file(scenario_path);
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  std::printf("validate: scenario='%s' solution solver='%s' status='%s'\n",
              sc.name.c_str(), sol.solver.c_str(),
              oc::solver_status_to_string(sol.status));
  if (vr.ok) {
    std::printf(
        "validate: OK recomputed total_cost=%.6f travel_time_min=%.3f "
        "travel_dist_km=%.3f vehicles_used=%d lateness_min=%.3f "
        "unserved=%d\n",
        vr.recomputed.total_cost, vr.recomputed.travel_time_min,
        vr.recomputed.travel_dist_km, vr.recomputed.vehicles_used,
        vr.recomputed.total_lateness_min, vr.recomputed.unserved_count);
    return 0;
  }
  for (const auto& v : vr.violations)
    std::printf("VIOLATION %s: %s\n", v.code.c_str(), v.message.c_str());
  std::printf("validate: FAILED with %zu violations\n", vr.violations.size());
  return 2;
}

int cmd_list_solvers() {
  std::printf("chronos-cpu\n");
  return 0;
}

int cmd_info() {
  std::printf("opticore %s, scenario format v%s\n", oc::kOptiCoreVersion,
              oc::kScenarioFormatVersion);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "%s", kUsage.c_str());
    return 1;
  }
  const std::string sub = argv[1];
  Flags flags;
  for (int i = 2; i < argc; ++i) flags.emplace_back(argv[i]);

  try {
    if (sub == "gen") return cmd_gen(flags);
    if (sub == "solve") return cmd_solve(flags);
    if (sub == "validate") return cmd_validate(flags);
    if (sub == "list-solvers") return cmd_list_solvers();
    if (sub == "info") return cmd_info();
    if (sub == "-h" || sub == "--help" || sub == "help") {
      std::printf("%s", kUsage.c_str());
      return 0;
    }
    std::fprintf(stderr, "unknown subcommand '%s'\n%s", sub.c_str(),
                 kUsage.c_str());
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}

