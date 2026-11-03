// OptiCore simulation module: deterministic synthetic city scenario
// generation (clustered Eurandom-style layout, time windows, demands).
#pragma once

#include "opticore/rng.hpp"
#include "opticore/scenario.hpp"

namespace oc {

struct CitySpec {
  std::string name = "city";
  uint64_t seed = 1;
  int customers = 16;
  int clusters = 3;
  double area_km = 20.0;        // square side length in km
  double speed_kmh = 30.0;      // free-flow speed
  double congestion = 0.0;      // additive multiplicative noise factor (mean 0)
  int vehicle_count = 8;
  double horizon_min = 600.0;   // customers' time windows live in [0, horizon]
  int min_demand = 2;
  int max_demand = 12;
  int min_service = 1;
  int max_service = 5;
  int min_window = 60;
  int max_window = 120;
  TimeWindowMode time_window_mode = TimeWindowMode::Hard;
  int capacity = 0;             // 0 = auto: ceil(0.9 * total_demand / vehicles)
  double max_route_duration = -1.0;
  ObjectiveWeights weights;
};

// Generate a deterministic Scenario from the spec. Same seed -> identical
// bytes on any platform/compiler (mt19937_64 is standardized).
Scenario generate_city_scenario(const CitySpec& spec);

}  // namespace oc
