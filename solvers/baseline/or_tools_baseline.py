# OptiCore OR-Tools reference baseline (CPU).
#
# Reads the frozen .ocx scenario format, solves the same CVRPTW with
# Google OR-Tools Routing (cheapest-arc + guided local search) and writes
# the frozen .ocs solution format with a mirrored objective breakdown.
#
# Weight/scale convention (documented in docs/ALGORITHMS.md):
#   All OR-Tools arc costs, penalties and bounds are scaled integers
#   (milli-units, SCALE = 1000). The claimed objective breakdown is NOT
#   read from OR-Tools: it is recomputed with a mirrored planner that is
#   numerically identical to the C++ validator, so solutions written here
#   are validated by the independent C++ validator without tolerance
#   games.
#
# Usage:
#   python solvers/baseline/or_tools_baseline.py --scenario FILE --solution FILE
#        [--time-limit-s 10] [--first-solution PATH_CHEAPEST_ARC | ...]
#        [--ls GUIDED_LOCAL_SEARCH | GREEDY_DESCENT | ...] [--log]
from __future__ import annotations

import argparse
import math
import pathlib
import sys
import time

from ortools.constraint_solver import pywrapcp, routing_enums_pb2

SCALE = 1000  # milli-minutes / milli-km resolution for OR-Tools internals


class ScenarioFormatError(ValueError):
    pass


class Scenario:
    def __init__(self):
        self.name = ""
        self.seed = 0
        self.time_window_mode = "hard"
        self.travel_model = "matrix"
        self.vehicle_start_time = 0.0
        self.vehicle_count = 1
        self.capacity = 1
        self.max_route_duration = -1.0
        self.weights = {
            "travel_time": 1.0,
            "travel_dist": 0.0,
            "vehicle_fixed": 100.0,
            "lateness": 500.0,
            "unserved": 10000.0,
        }
        self.nodes: list[dict] = []      # {x,y,demand,service,tw_open,tw_close}
        self.time_min: list[list[float]] = []
        self.dist_km: list[list[float]] = []

    @property
    def n(self) -> int:
        return len(self.nodes)


def parse_scenario(text: str) -> Scenario:
    sc = Scenario()
    state = "header"
    node_id = 0
    time_rows: list[list[float]] = []
    dist_rows: list[list[float]] = []
    seen: set[str] = set()
    weight_seen: set[str] = set()
    nodes_closed = False

    for raw in text.splitlines():
        line = raw.rstrip("\r")
        if not line or line.startswith("#"):
            continue
        ctx = line.split(" ", 1)

        if state == "header":
            key = ctx[0]
            if key in ("name", "travel_model"):
                if key == "name":
                    sc.name = ctx[1].strip()
                else:
                    sc.travel_model = ctx[1].strip()
                continue
            if key == "seed":
                seen.add("seed")
                sc.seed = int(ctx[1])
                continue
            if key == "time_window_mode":
                seen.add(key)
                sc.time_window_mode = ctx[1].strip()
                if sc.time_window_mode not in ("hard", "soft"):
                    raise ScenarioFormatError(f"bad time_window_mode: {ctx[1]}")
                continue
            if key == "vehicle_start_time":
                seen.add(key)
                sc.vehicle_start_time = float(ctx[1])
                continue
            if key == "vehicle_count":
                seen.add(key)
                sc.vehicle_count = int(ctx[1])
                continue
            if key == "capacity":
                seen.add(key)
                sc.capacity = int(ctx[1])
                continue
            if key == "max_route_duration":
                seen.add(key)
                sc.max_route_duration = float(ctx[1])
                continue
            if key.startswith("weight_"):
                weight_seen.add(key[len("weight_"):])
                sc.weights[key[len("weight_"):]] = float(ctx[1])
                continue
            if key == "BEGIN_NODES":
                required = {"seed", "time_window_mode", "vehicle_start_time",
                            "vehicle_count", "capacity", "max_route_duration"}
                if not required <= seen:
                    missing = required - seen
                    raise ScenarioFormatError(
                        f"missing header keys before BEGIN_NODES: {sorted(missing)}")
                if weight_seen != {"travel_time", "travel_dist",
                                   "vehicle_fixed", "lateness", "unserved"}:
                    raise ScenarioFormatError(
                        f"missing weight keys before BEGIN_NODES: "
                        f"{sorted({'travel_time', 'travel_dist', 'vehicle_fixed', 'lateness', 'unserved'} - weight_seen)}")
                state = "nodes"
                continue
            if key == "BEGIN_MATRIX_TIME_MIN":
                if not nodes_closed:
                    raise ScenarioFormatError(
                        "matrix sections must come after END_NODES")
                state = "matrix_time"
                continue
            if key == "BEGIN_MATRIX_DIST_KM":
                if not nodes_closed:
                    raise ScenarioFormatError(
                        "matrix sections must come after END_NODES")
                state = "matrix_dist"
                continue
            raise ScenarioFormatError(f"unexpected header line: {line}")

        if state == "nodes":
            if line == "END_NODES":
                if node_id < 1:
                    raise ScenarioFormatError("no nodes given")
                nodes_closed = True
                state = "header"
                continue
            cols = line.split()
            if len(cols) != 7:
                raise ScenarioFormatError(f"bad node row: {line}")
            if int(cols[0]) != node_id:
                raise ScenarioFormatError("node ids must be sequential")
            demand = int(cols[3])
            service, tw_open, tw_close = float(cols[4]), float(cols[5]), float(cols[6])
            if any(math.isnan(v) or math.isinf(v) for v in (service, tw_open, tw_close, float(cols[1]), float(cols[2]))):
                raise ScenarioFormatError("non-finite value in node row")
            if demand < 0 or service < 0 or tw_open < 0 or tw_close < tw_open:
                raise ScenarioFormatError(f"invalid node row: {line}")
            if node_id == 0 and demand != 0:
                raise ScenarioFormatError("depot must have demand 0")
            sc.nodes.append({
                "x": float(cols[1]), "y": float(cols[2]), "demand": demand,
                "service": service, "tw_open": tw_open, "tw_close": tw_close,
            })
            node_id += 1
            continue

        if state == "matrix_time":
            if line == "END_MATRIX_TIME_MIN":
                state = "header"
                continue
            time_rows.append([float(v) for v in line.split()])
            continue

        if state == "matrix_dist":
            if line == "END_MATRIX_DIST_KM":
                state = "header"
                continue
            dist_rows.append([float(v) for v in line.split()])
            continue

        raise ScenarioFormatError(f"unexpected line in state {state}: {line}")

    if not nodes_closed:
        raise ScenarioFormatError("missing nodes section")
    if not time_rows or not dist_rows:
        raise ScenarioFormatError("missing matrices")
    if len(time_rows) != sc.n or len(dist_rows) != sc.n:
        raise ScenarioFormatError("matrix row count mismatch")
    for row in time_rows + dist_rows:
        if len(row) != sc.n:
            raise ScenarioFormatError("matrix row width mismatch")
        for v in row:
            if math.isnan(v) or math.isinf(v) or v < 0:
                raise ScenarioFormatError("bad matrix value")
    sc.time_min = time_rows
    sc.dist_km = dist_rows
    if sc.vehicle_count < 1 or sc.capacity < 1:
        raise ScenarioFormatError("bad vehicle_count/capacity")
    if sc.vehicle_start_time < 0:
        raise ScenarioFormatError("bad vehicle_start_time")
    return sc


def load_scenario(path: str) -> Scenario:
    text = pathlib.Path(path).read_text(encoding="utf-8")
    return parse_scenario(text)


# ---------------------------------------------------------------------------
# mirrored planner (identical math to oc::plan_route in core/src/route_planner.cpp)

def plan_route(sc: Scenario, stops: list[int]) -> dict:
    if not stops:
        return {"load": 0, "travel_time": 0.0, "dist": 0.0, "lateness": 0.0,
                "wait": 0.0, "return_time": 0.0, "elapsed": 0.0, "service": 0.0,
                "closed": True}
    t = sc.vehicle_start_time
    last = 0
    travel = 0.0
    dist = 0.0
    lateness = 0.0
    wait = 0.0
    service = 0.0
    load = 0
    for nd in stops:
        node = sc.nodes[nd]
        leg = sc.time_min[last][nd]
        travel += leg
        dist += sc.dist_km[last][nd]
        t += leg
        if t < node["tw_open"]:
            wait += node["tw_open"] - t
            t = node["tw_open"]
        if t > node["tw_close"]:
            lateness += t - node["tw_close"]
        t += node["service"]
        service += node["service"]
        load += node["demand"]
        last = nd
    travel += sc.time_min[last][0]
    dist += sc.dist_km[last][0]
    t += sc.time_min[last][0]
    return {"load": load, "travel_time": travel, "dist": dist,
            "lateness": lateness, "wait": wait, "return_time": t,
            "elapsed": t - sc.vehicle_start_time, "service": service}


def evaluate(sc: Scenario, routes: list[list[int]], unserved: int) -> dict:
    b = {"travel_time": 0.0, "dist": 0.0, "lateness": 0.0, "vehicles": 0,
         "unserved": unserved, "cost": 0.0}
    for r in routes:
        if not r:
            continue
        m = plan_route(sc, r)
        b["travel_time"] += m["travel_time"]
        b["dist"] += m["dist"]
        b["lateness"] += m["lateness"]
        b["vehicles"] += 1
    w = sc.weights
    b["cost"] = (w["travel_time"] * b["travel_time"] +
                 w["travel_dist"] * b["dist"] +
                 w["vehicle_fixed"] * b["vehicles"] +
                 w["lateness"] * b["lateness"] +
                 w["unserved"] * unserved)
    return b


def solution_to_text(sc: Scenario, solver: str, seed: int, status: str,
                     routes: list[list[int]], unserved: dict[int, str]) -> str:
    b = evaluate(sc, routes, len(unserved))
    out = []
    out.append("# opticore solution v1")
    out.append(f"scenario {sc.name}")
    out.append(f"solver {solver}")
    out.append(f"seed {seed}")
    out.append(f"status {status}")
    out.append("BEGIN_ROUTES")
    for v, stops in enumerate(routes):
        if not stops:
            continue
        out.append("vehicle " + str(v) + " 0" +
                   "".join(" " + str(s) for s in stops) + " 0")
    out.append("END_ROUTES")
    out.append("BEGIN_UNSERVED")
    for node_id, reason in sorted(unserved.items()):
        out.append(f"{node_id} {reason}")
    out.append("END_UNSERVED")
    out.append("BEGIN_OBJECTIVE")
    out.append(f"travel_time_min {b['travel_time']:.3f}")
    out.append(f"travel_dist_km {b['dist']:.3f}")
    out.append(f"vehicles_used {b['vehicles']}")
    out.append(f"total_lateness_min {b['lateness']:.3f}")
    out.append(f"unserved_count {len(unserved)}")
    out.append(f"total_cost {b['cost']:.17g}")
    out.append("END_OBJECTIVE")
    return "\n".join(out) + "\n"


def solve_with_ortools(sc: Scenario, time_limit_s: float,
                       first_strategy: str, ls_strategy: str) -> tuple[list[list[int]], dict[int, str]]:
    n = sc.n
    K = sc.vehicle_count
    manager = pywrapcp.RoutingIndexManager(n, K, 0)
    routing = pywrapcp.RoutingModel(manager)

    def tt(i, j):
        a = manager.IndexToNode(i)
        b_ = manager.IndexToNode(j)
        return int(round(SCALE * (sc.weights["travel_time"] * sc.time_min[a][b_] +
                                  sc.weights["travel_dist"] * sc.dist_km[a][b_])))

    transit = routing.RegisterTransitCallback(tt)
    routing.SetArcCostEvaluatorOfAllVehicles(transit)

    # capacity dimension
    def demand_cb(i):
        return sc.nodes[manager.IndexToNode(i)]["demand"]

    demand_idx = routing.RegisterUnaryTransitCallback(demand_cb)
    routing.AddDimensionWithVehicleCapacity(
        demand_idx, 0, [sc.capacity] * K, True, "Capacity")

    # time dimension: cumul at a node == arrival time (start of service)
    horizon = 0.0
    for nd in sc.nodes:
        horizon = max(horizon, nd["tw_close"] + nd["service"])
    horizon_scale = int(math.ceil(SCALE * horizon)) + 1

    def time_cb(i, j):
        a = manager.IndexToNode(i)
        b_ = manager.IndexToNode(j)
        return int(round(SCALE * (sc.nodes[a]["service"] + sc.time_min[a][b_])))

    time_idx = routing.RegisterTransitCallback(time_cb)
    routing.AddDimension(time_idx, horizon_scale, horizon_scale, True, "Time")
    time_dim = routing.GetDimensionOrDie("Time")

    for node_idx in range(1, n):
        node = sc.nodes[node_idx]
        index = manager.NodeToIndex(node_idx)
        time_dim.CumulVar(index).SetRange(
            int(round(SCALE * node["tw_open"])),
            int(round(SCALE * node["tw_close"])),
        )
        if sc.time_window_mode == "soft":
            high = int(round(SCALE * node["tw_close"]))
            penalty = sc.weights["lateness"]
            if hasattr(time_dim, "SetCumulVarSoftUpperBoundWithLinearPenalty"):
                time_dim.SetCumulVarSoftUpperBoundWithLinearPenalty(
                    index, high, int(round(penalty)))
            else:
                # fallback: allow forced lateness by widening bounds x10
                time_dim.CumulVar(index).SetRange(
                    int(round(SCALE * node["tw_open"])),
                    int(round(SCALE * node["tw_open"] +
                              (node["tw_close"] - node["tw_open"] + 240.0))),
                )

    for v in range(K):
        routing.SetFixedCostOfVehicle(int(round(SCALE * sc.weights["vehicle_fixed"])), v)
        if sc.max_route_duration >= 0:
            time_dim.SetSpanUpperBoundForVehicle(
                int(round(SCALE * sc.max_route_duration)), v)

        # depot start time
        time_dim.CumulVar(routing.Start(v)).SetRange(
            int(round(SCALE * sc.vehicle_start_time)), horizon_scale)

    # allow dropping clients
    for node_idx in range(1, n):
        index = manager.NodeToIndex(node_idx)
        routing.AddDisjunction([index], int(round(SCALE * sc.weights["unserved"])))

    search = pywrapcp.DefaultRoutingSearchParameters()
    search.first_solution_strategy = getattr(
        routing_enums_pb2.FirstSolutionStrategy, first_strategy)
    search.local_search_metaheuristic = getattr(
        routing_enums_pb2.LocalSearchMetaheuristic, ls_strategy)
    search.time_limit.FromSeconds(max(1, int(math.ceil(time_limit_s))))
    search.log_search = False

    solution = routing.SolveWithParameters(search)
    if solution is None:
        return [], {i: "ortools failed to find any solution" for i in range(1, n)}

    routes: list[list[int]] = []
    for v in range(K):
        index = routing.Start(v)
        stops: list[int] = []
        while not routing.IsEnd(index):
            node = manager.IndexToNode(index)
            if node != 0:
                stops.append(node)
            index = solution.Value(routing.NextVar(index))
        routes.append(stops)

    served = {s for r in routes for s in r}
    unserved = {}
    for node_idx in range(1, n):
        if node_idx not in served:
            unserved[node_idx] = "dropped by OR-Tools (penalized disjunction)"
    return routes, unserved


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--solution", required=True)
    ap.add_argument("--time-limit-s", type=float, default=10.0)
    ap.add_argument("--first-solution", default="PATH_CHEAPEST_ARC")
    ap.add_argument("--ls", default="GUIDED_LOCAL_SEARCH")
    args = ap.parse_args()

    try:
        sc = load_scenario(args.scenario)
    except (ScenarioFormatError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    t0 = time.perf_counter()
    routes, unserved = solve_with_ortools(
        sc, args.time_limit_s, args.first_solution, args.ls)
    elapsed = time.perf_counter() - t0
    served = sum(len(r) for r in routes)

    status = "feasible"
    if served == 0 and len(sc.nodes) > 1:
        status = "infeasible"

    text = solution_to_text(sc, "ortools-9.15", sc.seed, status, routes, unserved)
    pathlib.Path(args.solution).write_text(text, encoding="utf-8")

    b = evaluate(sc, routes, len(unserved))
    print(f"solve: solver=ortools-9.15 status={status} total_cost={b['cost']:.6f} "
          f"travel_time_min={b['travel_time']:.3f} travel_dist_km={b['dist']:.3f} "
          f"vehicles_used={b['vehicles']} lateness_min={b['lateness']:.3f} "
          f"served={served} unserved={len(unserved)} wall_s={elapsed:.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
