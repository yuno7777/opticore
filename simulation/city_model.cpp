#include "city_model.hpp"

#include <algorithm>
#include <cmath>

namespace oc {
namespace {

double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

}  // namespace

Scenario generate_city_scenario(const CitySpec& spec) {
  Rng rng(spec.seed);
  Scenario sc;
  sc.seed = spec.seed;
  sc.name = spec.name;
  sc.time_window_mode = spec.time_window_mode;
  sc.vehicle_count = std::max(1, spec.vehicle_count);
  sc.max_route_duration = spec.max_route_duration;
  sc.weights = spec.weights;
  sc.travel_model = "synthetic-clustered-euclidean";

  const int K = sc.vehicle_count;
  const int C = std::max(1, spec.customers);
  const int NC = std::max(1, spec.clusters);

  // Cluster centers spread across the area (not in the depot neighborhood).
  std::vector<std::pair<double, double>> centers;
  centers.reserve(static_cast<size_t>(NC));
  for (int c = 0; c < NC; ++c) {
    const double cx = rng.uniform(0.2 * spec.area_km, 0.8 * spec.area_km);
    const double cy = rng.uniform(0.2 * spec.area_km, 0.8 * spec.area_km);
    centers.emplace_back(cx, cy);
  }

  // Depot node 0 near the area center.
  Node depot;
  depot.demand = 0;
  depot.service_min = 0;
  depot.tw_open = 0;
  depot.tw_close = spec.horizon_min + 240.0;
  sc.nodes.push_back(depot);

  // Customers.
  std::vector<std::pair<double, double>> pos;
  pos.push_back({spec.area_km / 2.0, spec.area_km / 2.0});  // depot position
  for (int i = 0; i < C; ++i) {
    const std::pair<double, double>& center =
        centers[rng.uniform_int(0, NC - 1)];
    // jitter radius 10-15% of area around the cluster center
    const double radius = 0.10 * spec.area_km * rng.uniform(0.6, 1.0);
    const double ang = rng.uniform(0.0, 6.283185307179586);
    double x = center.first + radius * std::cos(ang);
    double y = center.second + radius * std::sin(ang);
    x = std::clamp(x, 0.5, spec.area_km - 0.5);
    y = std::clamp(y, 0.5, spec.area_km - 0.5);
    pos.emplace_back(x, y);

    Node nd;
    nd.x_km = x;
    nd.y_km = y;
    nd.demand = rng.uniform_int(spec.min_demand, spec.max_demand);
    nd.service_min =
        static_cast<double>(rng.uniform_int(spec.min_service, spec.max_service));
    const int width = rng.uniform_int(spec.min_window, spec.max_window);
    const int open = rng.uniform_int(
        0, std::max(0, static_cast<int>(spec.horizon_min) - width - 15));
    nd.tw_open = static_cast<double>(open);
    nd.tw_close = static_cast<double>(open + width);
    sc.nodes.push_back(nd);
  }

  auto euclid = [](double x1, double y1, double x2, double y2) {
    const double dx = x1 - x2;
    const double dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
  };

  const int n = static_cast<int>(sc.nodes.size());
  sc.dist_km.assign(static_cast<size_t>(n) * n, 0.0);
  sc.time_min.assign(static_cast<size_t>(n) * n, 0.0);
  double total_demand = 0.0;
  for (int i = 1; i < n; ++i)
    total_demand += sc.nodes[static_cast<size_t>(i)].demand;

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      const double d = round3(euclid(pos[i].first, pos[i].second, pos[j].first,
                                     pos[j].second));
      const double base = 60.0 * d / spec.speed_kmh;
      const double t = base * (1.0 + spec.congestion);
      sc.dist_km[static_cast<size_t>(i) * n + j] = d;
      sc.time_min[static_cast<size_t>(i) * n + j] =
          (i == j) ? 0.0 : round3(std::max(0.0, t));
    }
  }

  if (spec.capacity > 0) {
    sc.capacity = spec.capacity;
  } else {
    const double loaded = total_demand * 0.9 / static_cast<double>(K);
    const int cap = static_cast<int>(std::ceil(loaded));
    int max_single = 0;
    for (int i = 1; i < n; ++i)
      max_single = std::max(max_single, sc.nodes[static_cast<size_t>(i)].demand);
    sc.capacity = std::max(cap, max_single);
  }

  return sc;
}

}  // namespace oc
