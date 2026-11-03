// Core unit tests: planner, objective, validator, formats, generator.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/route_planner.hpp"
#include "opticore/scenario.hpp"
#include "opticore/solution.hpp"
#include "opticore/validator.hpp"
#include "city_model.hpp"
#include "chronos.hpp"
#include "harness/tester.hpp"

namespace {

oc::Scenario make_simple_scenario() {
  // depot 0 with customers 1..3; times in minutes, distances km
  oc::Scenario sc;
  sc.name = "simple";
  sc.seed = 1;
  sc.vehicle_count = 2;
  sc.capacity = 20;
  sc.vehicle_start_time = 0;
  oc::ObjectiveWeights w;
  sc.weights = w;

  oc::Node depot;
  depot.tw_open = 0;
  depot.tw_close = 600;
  sc.nodes.push_back(depot);

  // customer 1: demand 5, service 2, window [10, 100]
  oc::Node c1;
  c1.demand = 5; c1.service_min = 2; c1.tw_open = 10; c1.tw_close = 100;
  sc.nodes.push_back(c1);
  // customer 2: demand 6, service 3, window [0, 200]
  oc::Node c2;
  c2.demand = 6; c2.service_min = 3; c2.tw_open = 0; c2.tw_close = 200;
  sc.nodes.push_back(c2);
  // customer 3: demand 9, service 1, window [20, 150]
  oc::Node c3;
  c3.demand = 9; c3.service_min = 1; c3.tw_open = 20; c3.tw_close = 150;
  sc.nodes.push_back(c3);

  const int n = 4;
  sc.dist_km.assign(n * n, 0.0);
  sc.time_min.assign(n * n, 0.0);
  const double dkm[4][4] = {
      {0, 3, 5, 6},
      {3, 0, 4, 7},
      {5, 4, 0, 3},
      {6, 7, 3, 0},
  };
  const int tmin[4][4] = {
      {0, 12, 20, 24},
      {12, 0, 16, 28},
      {20, 16, 0, 12},
      {24, 28, 12, 0},
  };
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      sc.dist_km[i * n + j] = dkm[i][j];
      sc.time_min[i * n + j] = tmin[i][j];
    }
  }
  return sc;
}

OC_TEST(test_planner_timing, "planner: route timing math") {
  const oc::Scenario sc = make_simple_scenario();
  // route: 0 -> 1 -> 2 -> 0
  const oc::RouteMetrics m = oc::plan_route(sc, {1, 2});
  // depart 0 at t=0, arrive 1 at 12 -> wait to 10 not needed (12 >= 10)
  // serve 12..14, leave to node 2 arrive 14+16=30 (>= 0 ok), serve 30..33,
  // return arrive 33+20 = 53
  OC_CHECK_NEAR(m.return_time_min, 53.0, 1e-9, "return time of route 1->2");
  OC_CHECK_NEAR(m.travel_time_min, 12 + 16 + 20, 1e-9, "pure travel time");
  OC_CHECK_NEAR(m.travel_dist_km, 3 + 4 + 5, 1e-9, "travel distance");
  OC_CHECK(m.load == 11, "route load");
  OC_CHECK(m.lateness_min == 0, "no lateness");
  OC_CHECK_NEAR(m.elapsed_min, 53.0, 1e-9, "elapsed from t=0");

  // waiting: route 0 -> 3 -> 0 (tw_open=20): arrive 24 >= 20, direct ok
  // -> serve 24..25 -> return 25+24=49
  const oc::RouteMetrics m3 = oc::plan_route(sc, {3});
  OC_CHECK_NEAR(m3.return_time_min, 49.0, 1e-9, "return time of route 3");
  OC_CHECK_NEAR(m3.travel_dist_km, 6 + 6, 1e-9, "dist for route 3");
  OC_CHECK(m3.load == 9, "load for route 3");

  // TW forcing lateness: temporarily make node 1 window [100, 110]
  oc::Scenario late = make_simple_scenario();
  late.nodes[1].tw_open = 100;
  late.nodes[1].tw_close = 110;
  const oc::RouteMetrics ml = oc::plan_route(late, {1});
  // arrive at 12 -> wait to 100; tw_close = 110 >= 100 -> no lateness
  OC_CHECK(ml.lateness_min == 0, "wait before tw_open is not lateness");
  OC_CHECK_NEAR(ml.return_time_min, 100 + 2 + 12, 1e-9, "return after waiting");
  OC_CHECK_NEAR(ml.wait_time_min, 88.0, 1e-9, "wait time recorded");

  oc::Scenario impossible = make_simple_scenario();
  impossible.nodes[1].tw_close = 5;  // arrive 12 > 5
  const oc::RouteMetrics mi = oc::plan_route(impossible, {1});
  OC_CHECK_NEAR(mi.lateness_min, 12 - 5, 1e-9, "lateness when late arrival");
  return true;
}

OC_TEST(test_objective_breakdown, "objective: breakdown math") {
  const oc::Scenario sc = make_simple_scenario();
  // two routes: {1,2} and {3}: travel_time = 48 + 48 = 96, dist = 12+12=24
  // vehicles_used = 2, lateness = 0
  const oc::EvaluatedSolution ev = oc::evaluate(sc, {{1, 2}, {3}}, 0);
  OC_CHECK_NEAR(ev.breakdown.travel_time_min, 96.0, 1e-9, "sum travel time");
  OC_CHECK_NEAR(ev.breakdown.travel_dist_km, 24.0, 1e-9, "sum dist");
  OC_CHECK(ev.breakdown.vehicles_used == 2, "vehicles used");
  OC_CHECK(ev.breakdown.unserved_count == 0, "served all");
  const double expected =
      1.0 * 96 + 0.0 * 24 + 100.0 * 2 + 500.0 * 0 + 10000.0 * 0;
  OC_CHECK_NEAR(ev.breakdown.total_cost, expected, 1e-9, "total cost");
  // unserved penalty
  const oc::EvaluatedSolution ev2 = oc::evaluate(sc, {{1, 2}}, 1);
  const double expected2 = 1.0 * 48 + 100.0 * 1 + 10000.0 * 1;
  OC_CHECK_NEAR(ev2.breakdown.total_cost, expected2, 1e-9, "cost w/ unserved");
  return true;
}

OC_TEST(test_scenario_roundtrip, "scenario: text roundtrip exact") {
  oc::CitySpec spec;
  spec.name = "rt-scenario";
  spec.seed = 42;
  spec.customers = 12;
  spec.clusters = 3;
  spec.vehicle_count = 4;
  const oc::Scenario original = oc::generate_city_scenario(spec);
  const std::string text = oc::scenario_to_text(original);
  const oc::Scenario reparsed = oc::scenario_from_text(text);
  OC_CHECK(reparsed.name == original.name, "name roundtrip");
  OC_CHECK(reparsed.seed == original.seed, "seed roundtrip");
  OC_CHECK(reparsed.time_window_mode == original.time_window_mode, "tw mode");
  OC_CHECK(reparsed.capacity == original.capacity, "capacity roundtrip");
  OC_CHECK(reparsed.vehicle_count == original.vehicle_count, "vehicle count");
  OC_CHECK(reparsed.node_count() == original.node_count(), "node count");
  bool exact = reparsed.time_min == original.time_min &&
               reparsed.dist_km == original.dist_km;
  OC_CHECK(exact, "matrix bit-exact roundtrip (print %.17g)");
  for (size_t i = 0; i < original.nodes.size(); ++i) {
    OC_CHECK(reparsed.nodes[i].demand == original.nodes[i].demand,
             "demand roundtrip");
    OC_CHECK(reparsed.nodes[i].tw_open == original.nodes[i].tw_open,
             "tw_open roundtrip");
    OC_CHECK(reparsed.nodes[i].tw_close == original.nodes[i].tw_close,
             "tw_close roundtrip");
    OC_CHECK(reparsed.nodes[i].service_min == original.nodes[i].service_min,
             "service roundtrip");
  }
  return true;
}

OC_TEST(test_scenario_sanity_rejects, "scenario: integrity rejections") {
  oc::Scenario sc = make_simple_scenario();
  OC_CHECK(oc::scenario_sanity(sc).empty(), "simple scenario is sane");
  sc.time_min[1 * 4 + 2] = -1;
  OC_CHECK(!oc::scenario_sanity(sc).empty(), "negative time rejected");
  sc = make_simple_scenario();
  sc.nodes[1].tw_close = 0;  // close < open (open=10)
  OC_CHECK(!oc::scenario_sanity(sc).empty(), "bad tw rejected");
  sc = make_simple_scenario();
  sc.nodes[0].demand = 3;
  OC_CHECK(!oc::scenario_sanity(sc).empty(), "depot demand rejected");
  sc = make_simple_scenario();
  sc.dist_km[1 * 4 + 1] = 0.25;
  OC_CHECK(!oc::scenario_sanity(sc).empty(), "nonzero diagonal rejected");
  sc = make_simple_scenario();
  sc.weights.lateness = -2;
  OC_CHECK(!oc::scenario_sanity(sc).empty(), "negative weight rejected");
  // parse errors
  bool threw = false;
  try {
    oc::scenario_from_text("name x\nvegetable_count 3\n");
  } catch (const oc::ScenarioDataError&) {
    threw = true;
  }
  OC_CHECK(threw, "unknown header key rejected");
  return true;
}

OC_TEST(test_validator_passes_valid, "validator: accepts valid solutions") {
  const oc::Scenario sc = make_simple_scenario();
  oc::Solution sol;
  sol.scenario_name = "simple";
  sol.solver = "hand";
  sol.status = oc::SolverStatus::Feasible;
  sol.routes = {{1, 2}, {3}};
  const oc::EvaluatedSolution ev = oc::evaluate(sc, sol.routes, 0);
  sol.claimed = ev.breakdown;
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  OC_CHECK(vr.ok, "valid solution passes");
  OC_CHECK(vr.violations.empty(), "no violations");
  OC_CHECK(vr.routes_used == 2, "routes used == 2");
  OC_CHECK_NEAR(vr.recomputed.total_cost, ev.breakdown.total_cost, 1e-9,
                "recomputed matches");
  return true;
}

OC_TEST(test_validator_catches_all, "validator: full rule battery") {
  const oc::Scenario sc = make_simple_scenario();

  // capacity violation on route {1,2,3} (load 20 == cap ok; add nothing) ->
  // force exceeded by lowering capacity
  oc::Scenario tight = make_simple_scenario();
  tight.capacity = 10;
  oc::Solution sol;
  sol.scenario_name = "simple";
  sol.status = oc::SolverStatus::Feasible;
  sol.routes = {{1, 2, 3}};
  sol.claimed = oc::evaluate(tight, sol.routes, 0).breakdown;
  auto vr = oc::validate_solution(tight, sol);
  bool found_cap = false;
  for (const auto& v : vr.violations)
    if (v.code == "capacity_exceeded") found_cap = true;
  OC_CHECK(found_cap, "capacity violation flagged");

  // duplicate service
  sol.routes = {{1, 2}, {1}};
  sol.claimed = oc::evaluate(sc, sol.routes, 0).breakdown;
  vr = oc::validate_solution(sc, sol);
  bool found_dup = false;
  for (const auto& v : vr.violations)
    if (v.code == "duplicate_service") found_dup = true;
  OC_CHECK(found_dup, "duplicate service flagged");

  // missing: neither served nor unserved
  sol.routes = {{1}};
  sol.claimed = oc::evaluate(sc, sol.routes,
                             static_cast<int>(sol.unserved.size())).breakdown;
  vr = oc::validate_solution(sc, sol);
  bool found_missing = false;
  for (const auto& v : vr.violations)
    if (v.code == "not_served_not_unserved") found_missing = true;
  OC_CHECK(found_missing, "missing request flagged");

  // unserved consistency: serve {1,2}, list 3 unserved -> valid
  oc::Solution sol3;
  sol3.scenario_name = "simple";
  sol3.status = oc::SolverStatus::Feasible;
  sol3.routes = {{1, 2}};
  sol3.unserved = {{3, "test reason"}};
  sol3.claimed = oc::evaluate(sc, sol3.routes, 1).breakdown;
  vr = oc::validate_solution(sc, sol3);
  OC_CHECK(vr.ok, "partial service with listed unserved is valid");

  // unserved the depot (demand 0) -> invalid entry
  oc::Solution sol4;
  sol4.scenario_name = "simple";
  sol4.status = oc::SolverStatus::Feasible;
  sol4.routes = {{1, 2}};
  sol4.unserved = {{0, "should not happen"}};
  sol4.claimed = oc::evaluate(sc, sol4.routes, 1).breakdown;
  vr = oc::validate_solution(sc, sol4);
  bool found_inv = false;
  for (const auto& v : vr.violations)
    if (v.code == "invalid_unserved_entry") found_inv = true;
  OC_CHECK(found_inv, "unserved non-customer flagged");

  // objective mismatch
  oc::Solution sol5;
  sol5.scenario_name = "simple";
  sol5.status = oc::SolverStatus::Feasible;
  sol5.routes = {{1, 2}, {3}};
  sol5.claimed = oc::evaluate(sc, sol5.routes, 0).breakdown;
  sol5.claimed.total_cost += 123.0;
  vr = oc::validate_solution(sc, sol5);
  bool found_obj = false;
  for (const auto& v : vr.violations)
    if (v.code == "objective_mismatch") found_obj = true;
  OC_CHECK(found_obj, "objective mismatch flagged");

  // route count over fleet
  oc::Scenario k1 = make_simple_scenario();
  k1.vehicle_count = 1;
  oc::Solution sol6;
  sol6.scenario_name = "simple";
  sol6.status = oc::SolverStatus::Feasible;
  sol6.routes = {{1}, {2}};  // two vehicle rows; only 1 allowed
  sol6.routes.push_back({3});
  k1.capacity = 30;
  sol6.claimed = oc::evaluate(k1, sol6.routes, 0).breakdown;
  vr = oc::validate_solution(k1, sol6);
  bool found_rows = false;
  for (const auto& v : vr.violations)
    if (v.code == "excess_vehicle_rows") found_rows = true;
  OC_CHECK(found_rows, "vehicle row overflow flagged");

  // hard TW violation
  oc::Scenario hard = make_simple_scenario();
  hard.vehicle_count = 3;  // allow one route per customer
  hard.nodes[1].tw_close = 5;  // arrival 12 > 5
  oc::Solution sol7;
  sol7.scenario_name = "simple";
  sol7.status = oc::SolverStatus::Feasible;
  sol7.routes = {{1}, {2}, {3}};
  sol7.claimed = oc::evaluate(hard, sol7.routes, 0).breakdown;
  vr = oc::validate_solution(hard, sol7);
  bool found_tw = false;
  for (const auto& v : vr.violations)
    if (v.code == "time_window_hard") found_tw = true;
  OC_CHECK(found_tw, "hard TW violation flagged");

  // soft TW mode: same lateness is allowed (penalized), no violation
  hard.time_window_mode = oc::TimeWindowMode::Soft;
  vr = oc::validate_solution(hard, sol7);
  OC_CHECK(vr.ok, "soft TW allows lateness");
  return true;
}

OC_TEST(test_solution_roundtrip, "solution: text roundtrip") {
  const oc::Scenario sc = make_simple_scenario();
  oc::Solution sol;
  sol.scenario_name = "simple";
  sol.solver = "test-writer";
  sol.seed = 7;
  sol.status = oc::SolverStatus::Feasible;
  sol.routes = {{1, 2}, {3}, {}};
  sol.unserved = {{3, "tw window override"}};  // intentionally inconsistent;
  // use consistent variant instead for the objective-claim check
  sol.unserved = {};
  sol.claimed = oc::evaluate(sc, sol.routes, 0).breakdown;
  const std::string text = oc::solution_to_text(sol);
  const oc::Solution parsed = oc::solution_from_text(text);
  OC_CHECK(parsed.scenario_name == sol.scenario_name, "scenario name");
  OC_CHECK(parsed.solver == sol.solver, "solver name");
  OC_CHECK(parsed.seed == sol.seed, "seed");
  OC_CHECK(parsed.status == sol.status, "status");
  OC_CHECK(parsed.claimed.total_cost == sol.claimed.total_cost,
           "total cost roundtrip");
  return true;
}

OC_TEST(test_generator_determinism, "generator: seed determinism (bytes)") {
  oc::CitySpec spec;
  spec.name = "det";
  spec.seed = 99;
  spec.customers = 8;
  spec.clusters = 2;
  spec.vehicle_count = 3;
  const std::string t1 = oc::scenario_to_text(oc::generate_city_scenario(spec));
  const std::string t2 = oc::scenario_to_text(oc::generate_city_scenario(spec));
  OC_CHECK(t1 == t2, "identical seed -> identical bytes");
  spec.seed = 100;
  const std::string t3 = oc::scenario_to_text(oc::generate_city_scenario(spec));
  OC_CHECK(t1 != t3, "different seed -> different bytes");
  return true;
}

OC_TEST(test_exhaustive_optimum_tiny, "exact: 3-customer tiny optimum") {
  const oc::Scenario sc = make_simple_scenario();  // cap 20, 2 vehicles
  const std::vector<oc::NodeId> customers = {1, 2, 3};

  // Brute force: every assignment of customers to 2 vehicles (or unserved),
  // every permutation of each vehicle's stop list. Feasibility: capacity +
  // hard time windows. Unserved customers always incur their penalty, so
  // the best uncovered option is 3 x 10000 and the optimal serving plan
  // wins.
  double best = 1e300;
  for (int assign = 0; assign < 27; ++assign) {
    std::vector<std::vector<oc::NodeId>> slots(2);
    std::vector<oc::NodeId> unserved;
    int a = assign;
    for (const oc::NodeId c : customers) {
      const int where = a % 3;
      a /= 3;
      if (where < 2)
        slots[static_cast<size_t>(where)].push_back(c);
      else
        unserved.push_back(c);
    }
    for (int p = 0; p < 2; ++p)
      std::sort(slots[static_cast<size_t>(p)].begin(),
                slots[static_cast<size_t>(p)].end());

    std::vector<std::vector<oc::NodeId>> perms0;
    std::vector<oc::NodeId> p0 = slots[0];
    do {
      perms0.push_back(p0);
    } while (std::next_permutation(p0.begin(), p0.end()));
    std::vector<std::vector<oc::NodeId>> perms1;
    std::vector<oc::NodeId> p1 = slots[1];
    do {
      perms1.push_back(p1);
    } while (std::next_permutation(p1.begin(), p1.end()));

    for (const auto& r0 : perms0) {
      for (const auto& r1 : perms1) {
        const oc::EvaluatedSolution ev =
            oc::evaluate(sc, {r0, r1}, static_cast<int>(unserved.size()));
        bool ok = true;
        for (const auto& met : ev.metrics) {
          if (met.load > sc.capacity) ok = false;
          if (met.lateness_min > 1e-9) ok = false;
        }
        for (const oc::NodeId u : unserved) {
          if (sc.nodes[static_cast<size_t>(u)].demand > sc.capacity)
            ok = false;  // cannot be served at all - never a valid plan
        }
        if (!ok) continue;
        best = std::min(best, ev.breakdown.total_cost);
      }
    }
  }
  OC_CHECK_NEAR(best, 164.0, 1e-6,
                "3-customer optimum: single route 1->2->3 (travel 64 + "
                "one vehicle fixed 100)");

  // chronos must reach the optimal value on this tiny instance
  oc::ChronosSolver solver;
  oc::SolverOptions opts;
  opts.seed = 1;
  const oc::Solution sol = solver.solve(sc, opts);
  const oc::ValidationResult vr = oc::validate_solution(sc, sol);
  if (!vr.ok) {
    for (const auto& v : vr.violations)
      std::fprintf(stderr, "  VIOLATION %s: %s\n", v.code.c_str(),
                   v.message.c_str());
  }
  OC_CHECK(vr.ok, "chronos solution must pass the independent validator");
  OC_CHECK_NEAR(sol.claimed.total_cost, best, 1e-6,
                "chronos must reach the optimum on the tiny instance");
  return true;
}

}  // namespace

int main() { return oc_test::run_all(nullptr); }
