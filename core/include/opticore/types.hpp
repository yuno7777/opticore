// OptiCore fundamental domain types and units.
//
// Units (documented contract, see docs/PROBLEM_FORMULATION.md):
//   - distance: kilometers (double)
//   - travel time, service time, time windows, lateness, elapsed route
//     time: minutes (double), measured from scenario time t = 0
//   - demand, capacity: integer load units
//   - objective cost: dimensionless "cost index" (weighted sum)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace oc {

// Node identifiers. Node index == position in Scenario::nodes.
// Convention: node 0 is the single depot (demand == 0).
// All other nodes with demand > 0 are delivery requests ("customers").
using NodeId = int32_t;
using VehicleId = int32_t;

// Node record for scenario file section BEGIN_NODES.
struct Node {
  double x_km = 0.0;          // geographic-ish coordinate (informational)
  double y_km = 0.0;
  int demand = 0;             // load units delivered at this node (0 for depot)
  double service_min = 0.0;   // service duration at this node
  double tw_open = 0.0;       // earliest service start, minutes
  double tw_close = 0.0;      // latest service start, minutes
};

enum class TimeWindowMode : int {
  Hard = 0,  // arrival after tw_close is a feasibility violation
  Soft = 1,  // lateness allowed, penalized in the objective
};

enum class SolverStatus : int {
  NotRun = 0,
  Feasible = 1,
  Infeasible = 2,
  TimeoutIncumbent = 3,
  Failed = 4,
};

const char* solver_status_to_string(SolverStatus s);
SolverStatus solver_status_from_string(const std::string& s);

const char* time_window_mode_to_string(TimeWindowMode m);
bool time_window_mode_from_string(const std::string& s, TimeWindowMode* out);

inline bool is_customer(const Node& n) { return n.demand > 0; }

// Objective weight configuration. The total cost is:
//   w_travel_time  * sum(travel time minutes over all used routes)
// + w_travel_dist  * sum(travel distance km over all used routes)
// + w_vehicle_fixed * (number of used vehicles)
// + w_lateness     * sum(lateness minutes over all routes)
// + w_unserved     * (number of unserved customers)
// Total cost is a dimensionless "cost index". All weights >= 0.
struct ObjectiveWeights {
  double travel_time = 1.0;
  double travel_dist = 0.0;
  double vehicle_fixed = 100.0;
  double lateness = 500.0;
  double unserved = 10000.0;
};

// Per-route scheduled metrics, computed by RoutePlanner.
// This is the single source of truth used by the solver's cost evaluation,
// the objective breakdown, and the independent validator.
struct RouteMetrics {
  std::vector<NodeId> stops;        // customers only, no depot
  int load = 0;                     // sum of demands
  double travel_time_min = 0.0;     // pure travel time (excludes waiting)
  double travel_dist_km = 0.0;
  double service_time_min = 0.0;
  double lateness_min = 0.0;        // sum of violations over stops
  double wait_time_min = 0.0;       // sum of waiting before tw_open
  double elapsed_min = 0.0;         // depot departure -> depot return
  double return_time_min = 0.0;     // absolute time at depot return
  bool feasible_capacity = true;    // load <= capacity
  bool feasible_time = true;        // lateness == 0 (hard-TW sense)
};

// Complete objective breakdown for one candidate solution.
struct ObjectiveBreakdown {
  double travel_time_min = 0.0;
  double travel_dist_km = 0.0;
  double total_lateness_min = 0.0;
  int vehicles_used = 0;
  int unserved_count = 0;
  double total_cost = 0.0;
};

}  // namespace oc
