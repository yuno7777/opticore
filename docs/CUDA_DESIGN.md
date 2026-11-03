# CUDA_DESIGN.md — CUDA Acceleration Architecture and Kernel Contracts

This document specifies the GPU acceleration design, kernel interfaces, memory layouts, build recipe, and break-even measurement methodology for OptiCore.

---

## 1. Status and Environment Reality

- **Hardware Present**: NVIDIA GeForce RTX 4060 Laptop GPU (AD107, 8 GB GDDR6 VRAM, Compute Capability 8.9).
- **Driver Present**: NVIDIA Driver 617.42 with CUDA 13.4 User Mode Driver (UMD).
- **Toolchain Status**: **Not installed**. Neither `nvcc` nor the required Microsoft Visual C++ (`cl.exe`) host compiler are installed on this system.
- **Project Policy**: Following the master build prompt non-negotiable rules, OptiCore refuses to fabricate GPU benchmark numbers or speedups. The GPU implementation is documented as an **optional build path**. All production numbers reported in `docs/` reflect verified CPU execution.

---

## 2. Host Build Recipe for CUDA Support

To compile the CUDA acceleration modules on Windows 11:

```powershell
# 1. Install Visual Studio 2022 Build Tools (MSVC host compiler required by nvcc)
winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --add Microsoft.VisualStudio.Workload.VCTools"

# 2. Install NVIDIA CUDA Toolkit <= 13.4 (matching 617.42 driver)
# Download official installer: https://developer.nvidia.com/cuda-toolkit-archive

# 3. Configure CMake with CUDA language enabled
cmake -S . -B build_cuda -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPTICORE_ENABLE_CUDA=ON
cmake --build build_cuda
```

---

## 3. Workload Design: Batch Candidate Route Evaluation

Vehicle routing metaheuristics (such as large neighborhood search or genetic algorithms) generate thousands of candidate route mutations per iteration. Evaluating whether a candidate route is feasible and computing its delta-cost is completely data-parallel across candidate routes.

### Structure-of-Arrays (SoA) Host-Device Contract

To maximize GPU coalesced memory access, candidate routes are packed into flat contiguous arrays:

```cpp
struct BatchCandidateInput {
    int num_candidates;                  // Total candidate routes to evaluate
    int max_stops;                       // Maximum stops per route
    const int* d_route_offsets;          // int[num_candidates + 1]: stop slice boundaries
    const int* d_node_sequence;          // int[total_stops]: concatenated stop IDs
    const int* d_node_demands;           // int[N]: demand per node
    const double* d_service_times;       // double[N]: service duration per node
    const double* d_tw_early;            // double[N]: earliest service start
    const double* d_tw_late;             // double[N]: latest service start
    const double* d_matrix_time;         // double[N * N]: row-major travel time matrix
    const double* d_matrix_dist;         // double[N * N]: row-major distance matrix
    int capacity;                        // Vehicle capacity Q
    double max_duration;                 // Route duration cap L
    ObjectiveWeights weights;            // Cost weights
};

struct BatchCandidateOutput {
    double* d_cost;                      // double[num_candidates]: total cost
    int* d_is_feasible;                  // int[num_candidates]: 1 if valid, 0 if violated
    int* d_violation_code;               // int[num_candidates]: rule failure ID
};
```

---

## 4. CUDA Kernel Architecture

Each CUDA block evaluates one candidate route:

```cuda
__global__ void evaluate_candidate_routes_kernel(
    const int* __restrict__ route_offsets,
    const int* __restrict__ node_sequence,
    const int* __restrict__ demands,
    const double* __restrict__ service_times,
    const double* __restrict__ tw_early,
    const double* __restrict__ tw_late,
    const double* __restrict__ matrix_time,
    const double* __restrict__ matrix_dist,
    int capacity,
    double max_duration,
    double w_time, double w_dist, double w_fixed, double w_late,
    double* __restrict__ out_costs,
    int* __restrict__ out_feasible,
    int num_candidates,
    int N
) {
    int candidate_idx = blockIdx.x;
    if (candidate_idx >= num_candidates) return;

    int start_offset = route_offsets[candidate_idx];
    int end_offset = route_offsets[candidate_idx + 1];
    int num_stops = end_offset - start_offset;

    // Fast reject on empty
    if (num_stops == 0) {
        if (threadIdx.x == 0) {
            out_costs[candidate_idx] = 0.0;
            out_feasible[candidate_idx] = 1;
        }
        return;
    }

    // Thread 0 executes forward schedule scan (num_stops is small, typically 10-30)
    if (threadIdx.x == 0) {
        int current_node = 0; // Depot start
        double current_time = 0.0;
        double total_time = 0.0;
        double total_dist = 0.0;
        double total_late = 0.0;
        int total_demand = 0;
        bool feasible = true;

        for (int i = 0; i < num_stops; ++i) {
            int next_node = node_sequence[start_offset + i];
            total_demand += demands[next_node];
            if (total_demand > capacity) {
                feasible = false;
                break;
            }

            double leg_time = __ldg(&matrix_time[current_node * N + next_node]);
            double leg_dist = __ldg(&matrix_dist[current_node * N + next_node]);
            total_time += leg_time;
            total_dist += leg_dist;

            double arr = current_time + leg_time;
            double early = __ldg(&tw_early[next_node]);
            double late = __ldg(&tw_late[next_node]);
            double b = (arr > early) ? arr : early;

            if (b > late) {
                total_late += (b - late);
            }

            double s = __ldg(&service_times[next_node]);
            current_time = b + s;
            current_node = next_node;
        }

        // Return leg to depot
        double ret_time = __ldg(&matrix_time[current_node * N + 0]);
        double ret_dist = __ldg(&matrix_dist[current_node * N + 0]);
        total_time += ret_time;
        total_dist += ret_dist;
        double elapsed = current_time + ret_time;

        if (max_duration > 0.0 && elapsed > max_duration) {
            feasible = false;
        }

        double cost = w_time * total_time + w_dist * total_dist + w_fixed + w_late * total_late;

        out_costs[candidate_idx] = cost;
        out_feasible[candidate_idx] = feasible ? 1 : 0;
    }
}
```

---

## 5. Break-Even Benchmark Methodology

When the CUDA toolchain is available, the benchmark protocol mandates measuring:

1. **Host-to-Device Transfer ($T_{\text{H2D}}$)**: Time to copy route offsets and node sequences.
2. **Kernel Execution Time ($T_{\text{Kernel}}$)**: Monitored via `cudaEventRecord` on high-precision streams.
3. **Device-to-Host Transfer ($T_{\text{D2H}}$)**: Time to retrieve objective costs and feasibility flags.
4. **End-to-End GPU Time**: $T_{\text{GPU}} = T_{\text{H2D}} + T_{\text{Kernel}} + T_{\text{D2H}}$.
5. **CPU Equivalent Time ($T_{\text{CPU}}$)**: Evaluated using OpenMP or single-threaded `core/route_planner.cpp` over identical batches.

**Expected Break-Even Crossover**:
Due to PCIe transfer latency ($\sim 10\text{–}20\,\mu\text{s}$) and kernel launch overhead ($\sim 5\,\mu\text{s}$), batch sizes under 256 candidates will favor CPU execution. True GPU acceleration is expected to emerge only for batch sizes exceeding $512\text{–}1024$ candidates.
