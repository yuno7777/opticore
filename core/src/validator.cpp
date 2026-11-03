#include "opticore/validator.hpp"

#include <map>
#include <string>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/route_planner.hpp"
#include "opticore/scenario.hpp"

namespace oc {

ValidationResult validate_solution(const Scenario& sc, const Solution& sol) {
  ValidationResult res;
  std::vector<Violation>& bad = res.violations;
  auto flag = [&bad](const std::string& code, const std::string& msg) {
    bad.push_back(Violation{code, msg});
  };

  const int n = sc.node_count();

  if (sol.scenario_name != sc.name)
    flag("scenario_name_mismatch",
         "solution scenario='" + sol.scenario_name + "' scenario file name='" +
             sc.name + "'");

  if (static_cast<int>(sol.routes.size()) > sc.vehicle_count)
    flag("excess_vehicle_rows",
         "solution has " + std::to_string(sol.routes.size()) +
             " vehicle rows but scenario allows " +
             std::to_string(sc.vehicle_count));

  std::vector<int> served_count(n, 0);

  for (VehicleId v = 0; v < static_cast<VehicleId>(sol.routes.size()); ++v) {
    const auto& stops = sol.routes[static_cast<size_t>(v)];
    if (stops.empty()) continue;
    const RouteMetrics m = plan_route(sc, stops);
    res.metrics.push_back(m);
    res.routes_used += 1;

    for (const NodeId id : stops) {
      if (id < 0 || id >= n) {
        flag("unknown_node", "vehicle " + std::to_string(v) + " stop node " +
                                 std::to_string(id));
        continue;
      }
      served_count[id] += 1;
      if (sc.nodes[static_cast<size_t>(id)].demand <= 0) {
        flag("invalid_interior_node",
             "vehicle " + std::to_string(v) + " serves node " +
                 std::to_string(id) + " with demand 0/depot");
      }
    }

    if (m.load > sc.capacity)
      flag("capacity_exceeded",
           "vehicle " + std::to_string(v) + " load " + std::to_string(m.load) +
               " > capacity " + std::to_string(sc.capacity));

    if (sc.time_window_mode == TimeWindowMode::Hard && m.lateness_min > 1e-9)
      flag("time_window_hard",
           "vehicle " + std::to_string(v) + " late by " +
               std::to_string(m.lateness_min) + " min (hard time-window mode)");

    if (sc.max_route_duration >= 0 &&
        m.elapsed_min > sc.max_route_duration + 1e-9)
      flag("route_duration_exceeded",
           "vehicle " + std::to_string(v) + " elapsed " +
               std::to_string(m.elapsed_min) + " > limit " +
               std::to_string(sc.max_route_duration));
  }

  for (NodeId id = 0; id < n; ++id) {
    if (served_count[id] > 1)
      flag("duplicate_service", "node " + std::to_string(id) + " served " +
                                    std::to_string(served_count[id]) + "x");
  }

  int cust_total = 0;
  for (NodeId id = 0; id < n; ++id) {
    if (sc.nodes[static_cast<size_t>(id)].demand > 0) ++cust_total;
  }

  // unserved set
  std::map<int, int> listed_unserved;
  for (const auto& entry : sol.unserved) {
    const NodeId id = entry.first;
    if (id < 0 || id >= n) {
      flag("invalid_unserved_entry",
           "unserved node " + std::to_string(id) + " out of range");
      continue;
    }
    const Node& nd = sc.nodes[static_cast<size_t>(id)];
    if (nd.demand <= 0)
      flag("invalid_unserved_entry", "node " + std::to_string(id) +
                                         " not a customer (demand 0/depot)");
    if (nd.demand > 0 && served_count[id] > 0)
      flag("unserved_but_served", "node " + std::to_string(id) +
                                      " listed unserved but present on a route");
    ++listed_unserved[id];
  }
  for (const auto& kv : listed_unserved) {
    if (kv.second > 1)
      flag("duplicate_unserved", "node " + std::to_string(kv.first) +
                                     " listed " + std::to_string(kv.second) +
                                     "x");
  }
  for (NodeId id = 0; id < n; ++id) {
    if (sc.nodes[static_cast<size_t>(id)].demand <= 0) continue;
    if (served_count[id] == 0 && listed_unserved.count(id) == 0)
      flag("not_served_not_unserved",
           "node " + std::to_string(id) + " is neither on a route nor listed "
                                          "as unserved");
  }

  int served_total = 0;
  for (NodeId id = 0; id < n; ++id) {
    if (sc.nodes[static_cast<size_t>(id)].demand > 0 && served_count[id] > 0)
      ++served_total;
  }
  const int unserved_recomputed = cust_total - served_total;

  if (cust_total > 0 && served_total == 0 &&
      (sol.status == SolverStatus::Feasible ||
       sol.status == SolverStatus::TimeoutIncumbent))
    flag("nothing_served",
         "status declares a usable solution but no customer is served");

  // Recompute objective from scratch.
  std::vector<std::vector<NodeId>> routes_interior;
  routes_interior.reserve(sol.routes.size());
  for (const auto& stops : sol.routes) routes_interior.push_back(stops);
  const EvaluatedSolution ev = evaluate(sc, routes_interior, unserved_recomputed);
  res.recomputed = ev.breakdown;

  const ObjectiveBreakdown& c = sol.claimed;
  const ObjectiveBreakdown& r = res.recomputed;
  if (!close_enough(c.travel_time_min, r.travel_time_min))
    flag("objective_mismatch",
         "travel_time_min claimed " + std::to_string(c.travel_time_min) +
             " recomputed " + std::to_string(r.travel_time_min));
  if (!close_enough(c.travel_dist_km, r.travel_dist_km))
    flag("objective_mismatch",
         "travel_dist_km claimed " + std::to_string(c.travel_dist_km) +
             " recomputed " + std::to_string(r.travel_dist_km));
  if (c.vehicles_used != r.vehicles_used)
    flag("objective_mismatch", "vehicles_used claimed " +
                                   std::to_string(c.vehicles_used) +
                                   " recomputed " +
                                   std::to_string(r.vehicles_used));
  if (!close_enough(c.total_lateness_min, r.total_lateness_min))
    flag("objective_mismatch",
         "total_lateness_min claimed " + std::to_string(c.total_lateness_min) +
             " recomputed " + std::to_string(r.total_lateness_min));
  if (c.unserved_count != r.unserved_count)
    flag("objective_mismatch", "unserved_count claimed " +
                                   std::to_string(c.unserved_count) +
                                   " recomputed " +
                                   std::to_string(r.unserved_count));
  if (!close_enough(c.total_cost, r.total_cost))
    flag("objective_mismatch", "total_cost claimed " +
                                   std::to_string(c.total_cost) +
                                   " recomputed " +
                                   std::to_string(r.total_cost));

  res.ok = bad.empty();
  return res;
}

}  // namespace oc
