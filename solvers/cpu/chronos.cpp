#include "chronos.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "opticore/objective.hpp"
#include "opticore/rng.hpp"
#include "opticore/route_planner.hpp"
#include "opticore/scenario.hpp"

namespace oc {
namespace {

using Routes = std::vector<std::vector<NodeId>>;
using Unserved = std::vector<std::pair<NodeId, std::string>>;

int unserved_count(const Unserved& u) { return static_cast<int>(u.size()); }

double cost_of(const Scenario& sc, const Routes& routes, int unserved_count_v) {
  return evaluate(sc, routes, unserved_count_v).breakdown.total_cost;
}

// Feasibility predicate for a single route under the scenario rules
// (hard capacity; hard or soft time windows; optional duration limit).
bool route_feasible(const Scenario& sc, const RouteMetrics& m) {
  if (m.load > sc.capacity) return false;
  if (sc.time_window_mode == TimeWindowMode::Hard && m.lateness_min > 1e-9)
    return false;
  if (sc.max_route_duration >= 0 &&
      m.elapsed_min > sc.max_route_duration + 1e-9)
    return false;
  return true;
}

// Cheapest feasible insertion of `node` across all routes of a plan.
// An empty vehicle starts a new route and incurs the vehicle fixed cost.
bool cheapest_insertion(const Scenario& sc, Routes& routes, NodeId node,
                        VehicleId* out_veh, size_t* out_pos) {
  double best_delta = 0.0;
  int best_vehicle = -1;
  size_t best_index = 0;

  for (VehicleId v = 0; v < static_cast<VehicleId>(routes.size()); ++v) {
    const auto& route = routes[static_cast<size_t>(v)];
    if (route.empty()) {
      std::vector<NodeId> trial = {node};
      const RouteMetrics after = plan_route(sc, trial);
      if (route_feasible(sc, after)) {
        const double delta =
            sc.weights.travel_time * after.travel_time_min +
            sc.weights.travel_dist * after.travel_dist_km +
            sc.weights.vehicle_fixed +
            sc.weights.lateness * after.lateness_min;
        if (best_vehicle < 0 || delta < best_delta - 1e-12) {
          best_delta = delta;
          best_vehicle = v;
          best_index = 0;
        }
      }
      continue;
    }
    for (size_t p = 0; p <= route.size(); ++p) {
      std::vector<NodeId> trial;
      trial.reserve(route.size() + 1);
      trial.insert(trial.end(), route.begin(),
                   route.begin() + static_cast<long>(p));
      trial.push_back(node);
      trial.insert(trial.end(), route.begin() + static_cast<long>(p),
                   route.end());
      const RouteMetrics after = plan_route(sc, trial);
      if (!route_feasible(sc, after)) continue;
      const RouteMetrics before = plan_route(sc, route);
      const double delta =
          sc.weights.travel_time *
              (after.travel_time_min - before.travel_time_min) +
          sc.weights.travel_dist *
              (after.travel_dist_km - before.travel_dist_km) +
          sc.weights.lateness *
              (after.lateness_min - before.lateness_min);
      if (best_vehicle < 0 || delta < best_delta - 1e-12) {
        best_delta = delta;
        best_vehicle = v;
        best_index = p;
      }
    }
  }

  if (best_vehicle < 0) return false;
  *out_veh = best_vehicle;
  *out_pos = best_index;
  return true;
}

void commit_insertion(Routes& routes, VehicleId veh, size_t pos, NodeId node) {
  routes[static_cast<size_t>(veh)].insert(
      routes[static_cast<size_t>(veh)].begin() + static_cast<long>(pos), node);
}

// Deterministic first-improvement local search:
//   1) relocate: (a, i) -> any other route or same route, one position
//   2) swap: exchange positions (a, i) <-> (b, j), same or different route
//   3) 2-opt: intra-route segment reversal
// Repeats until a full sweep produces no improving move.
bool local_search(const Scenario& sc, Routes& routes, int unserved_count_v) {
  bool any_improved = false;
  bool improved = true;
  while (improved) {
    improved = false;
    const double base = cost_of(sc, routes, unserved_count_v);

    // relocate
    for (VehicleId a = 0;
         a < static_cast<VehicleId>(routes.size()) && !improved; ++a) {
      auto& ra = routes[static_cast<size_t>(a)];
      for (size_t ai = 0; ai < ra.size() && !improved; ++ai) {
        const NodeId node_id = ra[ai];
        std::vector<NodeId> reduced = ra;
        reduced.erase(reduced.begin() + static_cast<long>(ai));

        // same vehicle first (deterministic order)
        for (size_t p = 0; p <= reduced.size() && !improved; ++p) {
          if (p == ai || p == ai + 1) continue;
          std::vector<NodeId> trial_route = reduced;
          trial_route.insert(trial_route.begin() + static_cast<long>(p),
                             node_id);
          const RouteMetrics m = plan_route(sc, trial_route);
          if (!route_feasible(sc, m)) continue;
          std::vector<NodeId> saved = routes[static_cast<size_t>(a)];
          routes[static_cast<size_t>(a)] = trial_route;
          const double c = cost_of(sc, routes, unserved_count_v);
          if (c < base - 1e-9) {
            improved = true;
            any_improved = true;
          } else {
            routes[static_cast<size_t>(a)] = saved;
          }
        }
        if (improved) break;
        // other vehicles
        for (VehicleId b = 0;
             b < static_cast<VehicleId>(routes.size()) && !improved; ++b) {
          if (b == a) continue;
          if (routes[static_cast<size_t>(b)].empty()) {
            // target empty vehicle: start new route from the reduced plan
            std::vector<NodeId> trial_b = {node_id};
            const RouteMetrics mb = plan_route(sc, trial_b);
            const RouteMetrics ma = plan_route(sc, reduced);
            if (!route_feasible(sc, ma) || !route_feasible(sc, mb)) continue;
            Routes trial = routes;
            trial[static_cast<size_t>(a)] = reduced;
            trial[static_cast<size_t>(b)] = trial_b;
            const double c =
                cost_of(sc, trial, unserved_count_v);
            if (c < base - 1e-9) {
              routes = std::move(trial);
              improved = true;
              any_improved = true;
            }
            continue;
          }
          const size_t rb_len = routes[static_cast<size_t>(b)].size();
          for (size_t p = 0; p <= rb_len && !improved; ++p) {
            Routes trial = routes;
            trial[static_cast<size_t>(a)] = reduced;
            trial[static_cast<size_t>(b)].insert(
                trial[static_cast<size_t>(b)].begin() + static_cast<long>(p),
                node_id);
            const RouteMetrics ma = plan_route(sc, trial[static_cast<size_t>(a)]);
            const RouteMetrics mb = plan_route(sc, trial[static_cast<size_t>(b)]);
            if (!route_feasible(sc, ma) || !route_feasible(sc, mb)) continue;
            const double c = cost_of(sc, trial, unserved_count_v);
            if (c < base - 1e-9) {
              routes = std::move(trial);
              improved = true;
              any_improved = true;
            }
          }
        }
      }
    }
    if (improved) continue;

    // swap
    for (VehicleId a = 0;
         a < static_cast<VehicleId>(routes.size()) && !improved; ++a) {
      const size_t len_a = routes[static_cast<size_t>(a)].size();
      for (size_t ai = 0; ai < len_a && !improved; ++ai) {
        for (VehicleId b = a;
             b < static_cast<VehicleId>(routes.size()) && !improved; ++b) {
          const size_t len_b = routes[static_cast<size_t>(b)].size();
          const size_t bi_start = (b == a) ? ai + 1 : 0;
          for (size_t bi = bi_start; bi < len_b && !improved; ++bi) {
            const NodeId node_a = routes[static_cast<size_t>(a)][ai];
            const NodeId node_b = routes[static_cast<size_t>(b)][bi];
            std::vector<NodeId> trial_a = routes[static_cast<size_t>(a)];
            std::vector<NodeId> trial_b = routes[static_cast<size_t>(b)];
            if (a == b) {
              std::swap(trial_a[ai], trial_a[bi]);
            } else {
              trial_a[ai] = node_b;
              trial_b[bi] = node_a;
            }
            const RouteMetrics ma = plan_route(sc, trial_a);
            const RouteMetrics mb = plan_route(sc, trial_b);
            if (!route_feasible(sc, ma) || !route_feasible(sc, mb)) continue;
            Routes trial = routes;
            trial[static_cast<size_t>(a)] = trial_a;
            trial[static_cast<size_t>(b)] = trial_b;
            const double c = cost_of(sc, trial, unserved_count_v);
            if (c < base - 1e-9) {
              routes = std::move(trial);
              improved = true;
              any_improved = true;
            }
          }
        }
      }
    }
    if (improved) continue;

    // 2-opt intra-route
    for (VehicleId a = 0;
         a < static_cast<VehicleId>(routes.size()) && !improved; ++a) {
      const size_t len_a = routes[static_cast<size_t>(a)].size();
      for (size_t i = 0; i < len_a && !improved; ++i) {
        for (size_t j = i + 1; j < len_a && !improved; ++j) {
          std::vector<NodeId> trial = routes[static_cast<size_t>(a)];
          std::reverse(trial.begin() + static_cast<long>(i),
                       trial.begin() + static_cast<long>(j + 1));
          const RouteMetrics m = plan_route(sc, trial);
          if (!route_feasible(sc, m)) continue;
          Routes candidate = routes;
          candidate[static_cast<size_t>(a)] = trial;
          const double c = cost_of(sc, candidate, unserved_count_v);
          if (c < base - 1e-9) {
            routes = std::move(candidate);
            improved = true;
            any_improved = true;
          }
        }
      }
    }
  }
  return any_improved;
}

int served_count(const Routes& routes) {
  int total = 0;
  for (const auto& r : routes) total += static_cast<int>(r.size());
  return total;
}

std::vector<NodeId> flatten(const Routes& routes) {
  std::vector<NodeId> all;
  for (const auto& r : routes)
    for (const NodeId node_id : r) all.push_back(node_id);
  return all;
}

void remove_node(Routes& routes, NodeId node_id, Unserved& unserved,
                 const std::string& reason) {
  for (auto& r : routes) {
    auto it = std::find(r.begin(), r.end(), node_id);
    if (it != r.end()) {
      r.erase(it);
      unserved.emplace_back(node_id, reason);
      return;
    }
  }
}

void remove_from_unserved(Unserved& u, NodeId node_id) {
  for (size_t i = 0; i < u.size(); ++i) {
    if (u[i].first == node_id) {
      u.erase(u.begin() + static_cast<long>(i));
      return;
    }
  }
}

struct Deadline {
  bool active = false;
  std::chrono::steady_clock::time_point end;
  bool expired() const {
    if (!active) return false;
    return std::chrono::steady_clock::now() >= end;
  }
};

}  // namespace

Solution ChronosSolver::solve(const Scenario& sc, const SolverOptions& opts) {
  Solution sol;
  sol.scenario_name = sc.name;
  sol.solver = name();
  sol.time_window_mode = sc.time_window_mode;
  sol.seed = opts.seed;

  const int n = sc.node_count();
  std::vector<NodeId> customers;
  for (NodeId id = 1; id < n; ++id) {
    if (sc.nodes[static_cast<size_t>(id)].demand > 0)
      customers.push_back(id);
  }

  // ---- 1. infeasible-alone screening ----------------------------------
  std::map<NodeId, std::string> unserved_reason;
  std::map<NodeId, bool> hard_infeasible;  // never insertable
  std::vector<NodeId> to_construct;
  for (const NodeId c : customers) {
    std::string reason;
    bool ok = true;
    if (sc.nodes[static_cast<size_t>(c)].demand > sc.capacity) {
      ok = false;
      reason = "demand exceeds vehicle capacity";
    }
    if (ok && sc.time_window_mode == TimeWindowMode::Hard) {
      const double arrival = sc.vehicle_start_time + sc.time(0, c);
      if (arrival > sc.nodes[static_cast<size_t>(c)].tw_close + 1e-9) {
        ok = false;
        reason = "unreachable before tw_close even when served alone";
      }
    }
    if (ok && sc.max_route_duration >= 0) {
      const double roundtrip = sc.time(0, c) +
                               sc.nodes[static_cast<size_t>(c)].service_min +
                               sc.time(c, 0);
      if (roundtrip > sc.max_route_duration + 1e-9) {
        ok = false;
        reason = "single-customer route exceeds max duration";
      }
    }
    if (!ok) {
      unserved_reason.emplace(c, reason);
      hard_infeasible.emplace(c, true);
    } else {
      to_construct.push_back(c);
    }
  }

  // ---- 2. Clarke-Wright savings construction ---------------------------
  Routes routes(static_cast<size_t>(sc.vehicle_count), std::vector<NodeId>{});
  {
    struct SR {
      std::vector<NodeId> stops;
      bool alive = true;
    };
    std::vector<SR> sroutes;
    sroutes.reserve(to_construct.size());
    std::vector<int> route_of(static_cast<size_t>(n), -1);
    for (const NodeId c : to_construct) {
      sroutes.push_back(SR{{c}, true});
      route_of[static_cast<size_t>(c)] =
          static_cast<int>(sroutes.size()) - 1;
    }

    std::vector<std::tuple<double, NodeId, NodeId>> sav;
    sav.reserve(to_construct.size() * to_construct.size());
    for (const NodeId i : to_construct) {
      for (const NodeId j : to_construct) {
        if (i == j) continue;
        const double s = sc.time(i, 0) + sc.time(0, j) - sc.time(i, j);
        sav.emplace_back(s, i, j);
      }
    }
    std::sort(sav.begin(), sav.end(),
              [](const std::tuple<double, NodeId, NodeId>& x,
                 const std::tuple<double, NodeId, NodeId>& y) {
                if (std::get<0>(x) != std::get<0>(y))
                  return std::get<0>(x) > std::get<0>(y);
                if (std::get<1>(x) != std::get<1>(y))
                  return std::get<1>(x) < std::get<1>(y);
                return std::get<2>(x) < std::get<2>(y);
              });

    for (const auto& s : sav) {
      const double value = std::get<0>(s);
      const NodeId ni = std::get<1>(s);
      const NodeId nj = std::get<2>(s);
      (void)value;
      const int ri = route_of[static_cast<size_t>(ni)];
      const int rj = route_of[static_cast<size_t>(nj)];
      if (ri < 0 || rj < 0 || ri == rj) continue;
      SR& ra = sroutes[static_cast<size_t>(ri)];
      SR& rb = sroutes[static_cast<size_t>(rj)];
      if (!ra.alive || !rb.alive) continue;
      if (ra.stops.back() != ni || rb.stops.front() != nj) continue;

      std::vector<NodeId> merged = ra.stops;
      merged.insert(merged.end(), rb.stops.begin(), rb.stops.end());
      const RouteMetrics m = plan_route(sc, merged);
      if (!route_feasible(sc, m)) continue;

      ra.stops = std::move(merged);
      for (const NodeId node_id : rb.stops)
        route_of[static_cast<size_t>(node_id)] = ri;
      rb.alive = false;
    }

    std::vector<std::vector<NodeId>> non_empty;
    for (auto& sr : sroutes) {
      if (sr.alive && !sr.stops.empty())
        non_empty.push_back(std::move(sr.stops));
    }

    if (non_empty.size() > static_cast<size_t>(sc.vehicle_count)) {
      std::sort(non_empty.begin(), non_empty.end(),
                [](const std::vector<NodeId>& x, const std::vector<NodeId>& y) {
                  return x.size() > y.size();
                });
      for (size_t r = static_cast<size_t>(sc.vehicle_count);
           r < non_empty.size(); ++r) {
        for (const NodeId node_id : non_empty[r]) {
          unserved_reason.emplace(
              node_id,
              "fleet limit exceeded during savings construction");
        }
      }
      non_empty.resize(static_cast<size_t>(sc.vehicle_count));
    }

    for (size_t v = 0; v < non_empty.size() &&
                            v < static_cast<size_t>(sc.vehicle_count);
         ++v) {
      routes[v] = std::move(non_empty[v]);
    }
  }

  // ---- 3. cheapest feasible insertion of leftovers ----------------------
  {
    std::vector<NodeId> pending(customers);
    std::sort(pending.begin(), pending.end());
    for (const NodeId c : pending) {
      bool on_route = false;
      for (const auto& r : routes)
        if (std::find(r.begin(), r.end(), c) != r.end()) on_route = true;
      if (on_route) continue;
      if (hard_infeasible.count(c) > 0) continue;
      VehicleId veh = 0;
      size_t pos = 0;
      if (cheapest_insertion(sc, routes, c, &veh, &pos)) {
        commit_insertion(routes, veh, pos, c);
        unserved_reason.erase(c);
      } else if (unserved_reason.count(c) == 0) {
        unserved_reason.emplace(c, "no feasible insertion found");
      }
    }
  }

  Unserved current_unserved;
  for (const auto& kv : unserved_reason)
    current_unserved.emplace_back(kv.first, kv.second);

  double current_cost = cost_of(sc, routes, unserved_count(current_unserved));
  local_search(sc, routes, unserved_count(current_unserved));
  current_cost = cost_of(sc, routes, unserved_count(current_unserved));

  // ---- 4. seeded iterated local search ----------------------------------
  Rng rng(opts.seed);
  Deadline deadline;
  if (opts.time_budget_s > 0.0) {
    deadline.active = true;
    deadline.end = std::chrono::steady_clock::now() +
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                       std::chrono::duration<double>(opts.time_budget_s));
  }

  Routes best_routes = routes;
  Unserved best_unserved = current_unserved;
  double best_cost = current_cost;

  long long phases =
      opts.max_iterations > 0 ? opts.max_iterations : 250;
  bool hit_deadline = false;

  for (long long phase = 0; phase < phases; ++phase) {
    if (deadline.expired()) {
      hit_deadline = true;
      break;
    }
    const int served = served_count(routes);
    if (served < 2 || routes.empty()) break;
      // destroy: pick d distinct served customers uniformly at random
      const int upper = std::min(served, 12);
      const int d = rng.uniform_int(2, upper);
      std::vector<NodeId> pool = flatten(routes);
      std::map<NodeId, bool> removed;
      int guard = 0;
      while (static_cast<int>(removed.size()) < d && guard < 1000) {
        ++guard;
        const NodeId pick = pool[rng.uniform_int(
            0, static_cast<int>(pool.size()) - 1)];
        removed.emplace(pick, true);
      }

      Routes trial = routes;
      Unserved trial_unserved = current_unserved;
      std::vector<NodeId> removed_list;
      removed_list.reserve(removed.size());
      for (const auto& kv : removed) removed_list.push_back(kv.first);
      for (const NodeId node_id : removed_list)
        remove_node(trial, node_id, trial_unserved,
                    "removed by ILS destroy step");

      // repair: destroyed + previously unserved (except hard-infeasible)
      std::vector<NodeId> repair_candidates;
      for (const NodeId rm : removed_list) repair_candidates.push_back(rm);
      for (const auto& entry : current_unserved)
        if (removed.count(entry.first) == 0)
          repair_candidates.push_back(entry.first);
      std::sort(repair_candidates.begin(), repair_candidates.end());
      repair_candidates.erase(
          std::unique(repair_candidates.begin(), repair_candidates.end()),
          repair_candidates.end());

      for (const NodeId node_id : repair_candidates) {
        if (hard_infeasible.count(node_id) > 0) continue;
        VehicleId veh = 0;
        size_t pos = 0;
        if (cheapest_insertion(sc, trial, node_id, &veh, &pos)) {
          commit_insertion(trial, veh, pos, node_id);
          remove_from_unserved(trial_unserved, node_id);
        }
      }

      local_search(sc, trial, unserved_count(trial_unserved));
      const double trial_cost =
          cost_of(sc, trial, unserved_count(trial_unserved));
      if (trial_cost < current_cost - 1e-9) {
        routes = std::move(trial);
        current_unserved = std::move(trial_unserved);
        current_cost = trial_cost;
        if (trial_cost < best_cost - 1e-9) {
          best_routes = routes;
          best_unserved = current_unserved;
          best_cost = trial_cost;
        }
      }
  }

  // ---- 5. emit -----------------------------------------------------------
  sol.routes = best_routes;
  sol.unserved = best_unserved;
  const EvaluatedSolution ev = evaluate(sc, best_routes, unserved_count(best_unserved));
  sol.claimed = ev.breakdown;

  const int svc = served_count(best_routes);
  if (svc == 0 && !customers.empty()) {
    sol.status = SolverStatus::Infeasible;
  } else if (hit_deadline) {
    sol.status = SolverStatus::TimeoutIncumbent;
  } else {
    sol.status = SolverStatus::Feasible;
  }
  return sol;
}

}  // namespace oc
