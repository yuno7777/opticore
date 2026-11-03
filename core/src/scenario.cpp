#include "opticore/scenario.hpp"

#include "opticore/version.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace oc {
namespace {

std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool starts_with(const std::string& s, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return s.size() >= n && std::memcmp(s.c_str(), prefix, n) == 0;
}

double parse_double(const std::string& tok, const std::string& ctx) {
  if (tok.empty()) throw ScenarioDataError(ctx + ": empty numeric token");
  char* end = nullptr;
  errno = 0;
  const double v = std::strtod(tok.c_str(), &end);
  if (end != tok.c_str() + tok.size())
    throw ScenarioDataError(ctx + ": bad numeric token '" + tok + "'");
  if (!std::isfinite(v))
    throw ScenarioDataError(ctx + ": non-finite value '" + tok + "'");
  return v;
}

int parse_int(const std::string& tok, const std::string& ctx) {
  if (tok.empty()) throw ScenarioDataError(ctx + ": empty integer token");
  char* end = nullptr;
  errno = 0;
  const long v = std::strtol(tok.c_str(), &end, 10);
  if (end != tok.c_str() + tok.size() || errno == ERANGE)
    throw ScenarioDataError(ctx + ": bad integer token '" + tok + "'");
  if (v < -(1L << 30) || v > (1L << 30))
    throw ScenarioDataError(ctx + ": integer out of range '" + tok + "'");
  return static_cast<int>(v);
}

uint64_t parse_u64(const std::string& tok, const std::string& ctx) {
  if (tok.empty()) throw ScenarioDataError(ctx + ": empty unsigned token");
  char* end = nullptr;
  errno = 0;
  const unsigned long long v = std::strtoull(tok.c_str(), &end, 10);
  if (end != tok.c_str() + tok.size() || errno == ERANGE)
    throw ScenarioDataError(ctx + ": bad unsigned token '" + tok + "'");
  return static_cast<uint64_t>(v);
}

std::string fmt_double(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return std::string(buf);
}

std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> toks;
  std::istringstream ss(line);
  std::string t;
  while (ss >> t) toks.push_back(t);
  return toks;
}

}  // namespace

Scenario scenario_from_text(const std::string& text) {
  Scenario sc;
  std::istringstream in(text);
  std::string line;

  enum class Section { Header, Nodes, MatrixTime, MatrixDist } section =
      Section::Header;

  bool seen_name = false;
  bool seen_seed = false;
  bool seen_twm = false;
  bool seen_start = false;
  bool seen_vehicles = false;
  bool seen_capacity = false;
  bool seen_duration = false;
  bool seen_w_time = false;
  bool seen_w_dist = false;
  bool seen_w_fixed = false;
  bool seen_w_late = false;
  bool seen_w_uns = false;

  bool nodes_open = false;
  bool time_open = false;
  bool dist_open = false;
  bool nodes_closed = false;
  bool time_closed = false;
  bool dist_closed = false;
  int next_node_id = -1;

  std::vector<std::vector<double>> time_rows;
  std::vector<std::vector<double>> dist_rows;

  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::string ctx = "scenario line " + std::to_string(line_no);

    if (line.empty() || line[0] == '#') continue;

    if (section == Section::Header) {
      if (starts_with(line, "name ")) {
        sc.name = trim(line.substr(5));
        seen_name = true;
        continue;
      }
      if (starts_with(line, "travel_model ")) {
        sc.travel_model = trim(line.substr(13));
        continue;
      }
      if (starts_with(line, "seed ")) {
        sc.seed = parse_u64(trim(line.substr(5)), ctx);
        seen_seed = true;
        continue;
      }
      if (starts_with(line, "time_window_mode ")) {
        const std::string m = trim(line.substr(17));
        if (!time_window_mode_from_string(m, &sc.time_window_mode))
          throw ScenarioDataError(ctx + ": bad time_window_mode '" + m + "'");
        seen_twm = true;
        continue;
      }
      if (starts_with(line, "vehicle_start_time ")) {
        sc.vehicle_start_time = parse_double(trim(line.substr(19)), ctx);
        seen_start = true;
        continue;
      }
      if (starts_with(line, "vehicle_count ")) {
        sc.vehicle_count = parse_int(trim(line.substr(14)), ctx);
        seen_vehicles = true;
        continue;
      }
      if (starts_with(line, "capacity ")) {
        sc.capacity = parse_int(trim(line.substr(9)), ctx);
        seen_capacity = true;
        continue;
      }
      if (starts_with(line, "max_route_duration ")) {
        sc.max_route_duration = parse_double(trim(line.substr(19)), ctx);
        seen_duration = true;
        continue;
      }
      if (starts_with(line, "weight_travel_time ")) {
        sc.weights.travel_time = parse_double(trim(line.substr(19)), ctx);
        seen_w_time = true;
        continue;
      }
      if (starts_with(line, "weight_travel_dist ")) {
        sc.weights.travel_dist = parse_double(trim(line.substr(19)), ctx);
        seen_w_dist = true;
        continue;
      }
      if (starts_with(line, "weight_vehicle_fixed ")) {
        sc.weights.vehicle_fixed = parse_double(trim(line.substr(21)), ctx);
        seen_w_fixed = true;
        continue;
      }
      if (starts_with(line, "weight_lateness ")) {
        sc.weights.lateness = parse_double(trim(line.substr(16)), ctx);
        seen_w_late = true;
        continue;
      }
      if (starts_with(line, "weight_unserved ")) {
        sc.weights.unserved = parse_double(trim(line.substr(16)), ctx);
        seen_w_uns = true;
        continue;
      }
      if (line == "BEGIN_NODES") {
        if (!seen_seed || !seen_vehicles || !seen_capacity || !seen_twm ||
            !seen_start || !seen_duration || !seen_w_time || !seen_w_dist ||
            !seen_w_fixed || !seen_w_late || !seen_w_uns)
          throw ScenarioDataError(
              ctx + ": BEGIN_NODES requires all header scalar keys first");
        if (!seen_name) throw ScenarioDataError(ctx + ": missing 'name'");
        section = Section::Nodes;
        nodes_open = true;
        next_node_id = 0;
        continue;
      }
      if (line == "BEGIN_MATRIX_TIME_MIN") {
        if (!nodes_closed)
          throw ScenarioDataError(
              ctx + ": matrix sections must come after END_NODES");
        section = Section::MatrixTime;
        time_open = true;
        continue;
      }
      if (line == "BEGIN_MATRIX_DIST_KM") {
        if (!nodes_closed)
          throw ScenarioDataError(
              ctx + ": matrix sections must come after END_NODES");
        section = Section::MatrixDist;
        dist_open = true;
        continue;
      }
      throw ScenarioDataError(ctx + ": unexpected header line '" + line + "'");
    }

    if (section == Section::Nodes) {
      if (line == "END_NODES") {
        if (next_node_id < 1) throw ScenarioDataError(ctx + ": no nodes given");
        nodes_open = false;
        nodes_closed = true;
        section = Section::Header;
        continue;
      }
      const auto toks = tokenize(line);
      if (toks.size() != 7)
        throw ScenarioDataError(
            ctx + ": node line must have 7 columns (id x_km y_km demand "
                  "service_min tw_open tw_close)");
      if (parse_int(toks[0], ctx) != next_node_id)
        throw ScenarioDataError(ctx + ": node id must be sequential");
      Node nd;
      nd.x_km = parse_double(toks[1], ctx);
      nd.y_km = parse_double(toks[2], ctx);
      const int demand = parse_int(toks[3], ctx);
      if (demand < 0) throw ScenarioDataError(ctx + ": demand must be >= 0");
      nd.demand = demand;
      nd.service_min = parse_double(toks[4], ctx);
      nd.tw_open = parse_double(toks[5], ctx);
      nd.tw_close = parse_double(toks[6], ctx);
      if (nd.service_min < 0)
        throw ScenarioDataError(ctx + ": service_min must be >= 0");
      if (nd.tw_open < 0 || nd.tw_close < nd.tw_open)
        throw ScenarioDataError(ctx + ": bad time window");
      if (next_node_id == 0 && nd.demand != 0)
        throw ScenarioDataError(ctx + ": depot (node 0) must have demand 0");
      sc.nodes.push_back(nd);
      ++next_node_id;
      continue;
    }

    if (section == Section::MatrixTime) {
      if (line == "END_MATRIX_TIME_MIN") {
        time_open = false;
        time_closed = true;
        section = Section::Header;
        continue;
      }
      if (!nodes_closed)
        throw ScenarioDataError(ctx + ": matrices must come after nodes");
      std::vector<double> row;
      std::istringstream ss(line);
      std::string tok;
      while (ss >> tok) row.push_back(parse_double(tok, ctx));
      time_rows.push_back(std::move(row));
      continue;
    }

    if (section == Section::MatrixDist) {
      if (line == "END_MATRIX_DIST_KM") {
        dist_open = false;
        dist_closed = true;
        section = Section::Header;
        continue;
      }
      if (!nodes_closed)
        throw ScenarioDataError(ctx + ": matrices must come after nodes");
      std::vector<double> row;
      std::istringstream ss(line);
      std::string tok;
      while (ss >> tok) row.push_back(parse_double(tok, ctx));
      dist_rows.push_back(std::move(row));
      continue;
    }
  }

  if (nodes_open || time_open || dist_open)
    throw ScenarioDataError("scenario: unterminated section at end of file");
  if (!nodes_closed || !time_closed || !dist_closed)
    throw ScenarioDataError("scenario: missing required section");

  const int n = static_cast<int>(sc.nodes.size());
  if (sc.vehicle_count < 1) throw ScenarioDataError("scenario: vehicle_count < 1");
  if (sc.capacity < 1) throw ScenarioDataError("scenario: capacity < 1");
  if (sc.vehicle_start_time < 0)
    throw ScenarioDataError("scenario: vehicle_start_time must be >= 0");
  if (n > 0) {
    if (static_cast<int>(time_rows.size()) != n ||
        static_cast<int>(dist_rows.size()) != n)
      throw ScenarioDataError(
          "scenario: matrix row count must equal node count");
    sc.time_min.resize(static_cast<size_t>(n) * static_cast<size_t>(n));
    sc.dist_km.resize(static_cast<size_t>(n) * static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      if (static_cast<int>(time_rows[i].size()) != n ||
          static_cast<int>(dist_rows[i].size()) != n)
        throw ScenarioDataError(
            "scenario: matrix row width must equal node count");
      for (int j = 0; j < n; ++j) {
        const size_t idx = static_cast<size_t>(i) * static_cast<size_t>(n) +
                           static_cast<size_t>(j);
        sc.time_min[idx] = time_rows[i][j];
        sc.dist_km[idx] = dist_rows[i][j];
      }
    }
  }
  return sc;
}

std::vector<std::string> scenario_sanity(const Scenario& sc) {
  std::vector<std::string> problems;
  const int n = sc.node_count();
  if (n < 2) problems.push_back("scenario needs depot + at least one node");
  if (sc.name.empty()) problems.push_back("scenario name is empty");
  if (sc.vehicle_count < 1) problems.push_back("vehicle_count < 1");
  if (sc.capacity < 1) problems.push_back("capacity < 1");
  if (sc.vehicle_start_time < 0)
    problems.push_back("vehicle_start_time < 0");
  if (sc.max_route_duration >= 0 && sc.max_route_duration <= 0)
    problems.push_back("max_route_duration must be positive or -1");
  if (sc.weights.travel_time < 0 || sc.weights.travel_dist < 0 ||
      sc.weights.vehicle_fixed < 0 || sc.weights.lateness < 0 ||
      sc.weights.unserved < 0)
    problems.push_back("objective weights must be >= 0");
  if (static_cast<int>(sc.nodes.size()) < 2) return problems;

  const Node& depot = sc.nodes[0];
  if (depot.demand != 0)
    problems.push_back("depot (node 0) must have demand 0");
  if (depot.service_min < 0) problems.push_back("depot service_min < 0");

  for (int i = 0; i < n; ++i) {
    const Node& nd = sc.nodes[static_cast<size_t>(i)];
    if (nd.demand < 0) problems.push_back("negative demand at node " + std::to_string(i));
    if (nd.service_min < 0)
      problems.push_back("negative service_min at node " + std::to_string(i));
    if (nd.tw_open < 0)
      problems.push_back("negative tw_open at node " + std::to_string(i));
    if (nd.tw_close < nd.tw_open)
      problems.push_back("tw_close < tw_open at node " + std::to_string(i));
  }

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      const double t = sc.time(i, j);
      const double d = sc.dist(i, j);
      if (t < 0 || d < 0) {
        problems.push_back("negative travel value at (" + std::to_string(i) +
                           "," + std::to_string(j) + ")");
      }
      if (i == j && (t != 0 || d != 0)) {
        problems.push_back("matrix diagonal must be zero");
      }
    }
  }
  return problems;
}

Scenario scenario_from_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw ScenarioDataError("cannot open scenario file: " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return scenario_from_text(ss.str());
}

std::string scenario_to_text(const Scenario& sc) {
  std::ostringstream o;
  o << "# opticore scenario v" << kScenarioFormatVersion << "\n";
  o << "name " << sc.name << "\n";
  o << "seed " << sc.seed << "\n";
  o << "time_window_mode " << time_window_mode_to_string(sc.time_window_mode)
    << "\n";
  o << "travel_model " << sc.travel_model << "\n";
  o << "vehicle_start_time " << fmt_double(sc.vehicle_start_time) << "\n";
  o << "vehicle_count " << sc.vehicle_count << "\n";
  o << "capacity " << sc.capacity << "\n";
  o << "max_route_duration " << fmt_double(sc.max_route_duration) << "\n";
  o << "weight_travel_time " << fmt_double(sc.weights.travel_time) << "\n";
  o << "weight_travel_dist " << fmt_double(sc.weights.travel_dist) << "\n";
  o << "weight_vehicle_fixed " << fmt_double(sc.weights.vehicle_fixed) << "\n";
  o << "weight_lateness " << fmt_double(sc.weights.lateness) << "\n";
  o << "weight_unserved " << fmt_double(sc.weights.unserved) << "\n";

  const int n = sc.node_count();
  o << "BEGIN_NODES\n";
  o << "# id x_km y_km demand service_min tw_open tw_close\n";
  for (int i = 0; i < n; ++i) {
    const Node& nd = sc.nodes[static_cast<size_t>(i)];
    o << i << " " << fmt_double(nd.x_km) << " " << fmt_double(nd.y_km) << " "
      << nd.demand << " " << fmt_double(nd.service_min) << " "
      << fmt_double(nd.tw_open) << " " << fmt_double(nd.tw_close) << "\n";
  }
  o << "END_NODES\n";

  o << "BEGIN_MATRIX_TIME_MIN\n";
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      o << (j ? " " : "") << fmt_double(sc.time(i, j));
    }
    o << "\n";
  }
  o << "END_MATRIX_TIME_MIN\n";

  o << "BEGIN_MATRIX_DIST_KM\n";
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      o << (j ? " " : "") << fmt_double(sc.dist(i, j));
    }
    o << "\n";
  }
  o << "END_MATRIX_DIST_KM\n";
  return o.str();
}

}  // namespace oc
