#include "opticore/route_planner.hpp"

#include "opticore/scenario.hpp"

namespace oc {

RouteMetrics plan_route(const Scenario& sc, const std::vector<NodeId>& stops) {
  RouteMetrics m;
  m.stops = stops;
  if (stops.empty()) return m;

  const int n = static_cast<int>(stops.size());
  double t = sc.vehicle_start_time;
  NodeId last = 0;  // depot
  double pure_travel = 0.0;
  double dist_sum = 0.0;
  double lateness = 0.0;
  double wait = 0.0;
  double service_sum = 0.0;

  for (int i = 0; i < n; ++i) {
    const NodeId node = stops[static_cast<size_t>(i)];
    const Node& nd = sc.nodes[static_cast<size_t>(node)];
    const double leg = sc.time(last, node);
    const double leg_km = sc.dist(last, node);
    pure_travel += leg;
    dist_sum += leg_km;
    t += leg;
    if (t < nd.tw_open) {
      wait += nd.tw_open - t;
      t = nd.tw_open;
    }
    if (t > nd.tw_close) {
      lateness += t - nd.tw_close;
    }
    t += nd.service_min;
    service_sum += nd.service_min;
    m.load += nd.demand;
    last = node;
  }

  const double leg_home = sc.time(last, 0);
  const double leg_home_km = sc.dist(last, 0);
  pure_travel += leg_home;
  dist_sum += leg_home_km;
  t += leg_home;

  m.travel_time_min = pure_travel;
  m.travel_dist_km = dist_sum;
  m.service_time_min = service_sum;
  m.lateness_min = lateness;
  m.wait_time_min = wait;
  m.return_time_min = t;
  m.elapsed_min = t - sc.vehicle_start_time;
  m.feasible_capacity = m.load <= sc.capacity;
  m.feasible_time = lateness == 0.0;
  return m;
}

}  // namespace oc
