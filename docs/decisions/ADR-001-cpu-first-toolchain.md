# ADR-001: CPU-First Implementation with Documented Optional CUDA Build

## Status
Accepted

## Context
Phase 0 environment reconnaissance revealed that the host machine (Windows 11 Home, i7-13620H) has an NVIDIA GeForce RTX 4060 Laptop GPU with driver 617.42 supporting CUDA 13.4 at the driver level, but no CUDA Toolkit (no `nvcc`) and no Visual Studio / MSVC (`cl.exe`) host compiler. On Windows, `nvcc` strictly requires MSVC for host compilation; MinGW GCC (MSYS2) cannot be used as an `nvcc` host compiler. Installing the MSVC toolchain and CUDA toolkit requires several gigabytes of downloads and administrator privileges.

The OptiCore operating contract mandates that GPU acceleration is a hypothesis to test rather than a requirement to claim, and explicitly directs: *"If CUDA is unavailable, keep the CPU solver fully functional and document the optional GPU build."*

## Decision
1. Proceed with a fully functional, high-performance CPU implementation using GCC 16.2.0 (MSYS2 UCRT64), CMake 4.4.4, and Ninja 1.13.2.
2. Formulate the CUDA acceleration design, memory layout (SoA), kernel interfaces, and break-even measurement methodology in `docs/CUDA_DESIGN.md`.
3. Provide a clear recipe for users with MSVC + CUDA to enable the optional GPU build (`-DOPTICORE_ENABLE_CUDA=ON`).
4. Prohibit any fabricated GPU speedup numbers; report only verified CPU benchmarks in documentation.

## Consequences
- **Positive**: Clean, zero-admin build; zero external binary bloat; fully reproducible results on any standard C++20 platform.
- **Negative**: GPU kernels are not compiled or profiled on this machine; speedup claims are deferred until hardware with the proper toolchain is available.
