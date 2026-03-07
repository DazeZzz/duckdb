# Phase-Aware Scheduler Experimental Data Generation

This directory contains tools for generating experimental data for the phase-aware scheduler paper.

## Overview

The benchmark framework simulates TPC-H query execution with different scheduling strategies across 6 phases:
- **Phase 0**: Baseline (no optimizations)
- **Phase 1**: Phase-aware scheduling (Inelastic-First strategy)
- **Phase 2**: Adaptive morsel execution
- **Phase 3**: Adaptive priorities
- **Phase 4**: Lock-free scheduler
- **Phase 5**: Self-tuning configuration

## Files

- `benchmark_framework.hpp/cpp` - Core benchmark framework
- `generate_phase0_data.cpp` - Phase 0 baseline data generation
- `test_benchmark_phase0.cpp` - Unit tests for benchmark framework

## Generated Data

Each phase generates two types of files:

### 1. Aggregated Results (CSV)
Contains statistical summaries across multiple runs:
- Average and standard deviation of response time
- Average and standard deviation of throughput
- Scheduling overhead
- Parallel efficiency
- Latency percentiles (P50, P90, P99)
- Slowdown compared to baseline

### 2. Detailed Logs (JSON)
Contains per-query execution details:
- Task execution times
- Morsel sizes
- Phase types (ELASTIC/INELASTIC)
- Scheduling events

## TPC-H Query Mix

The framework uses 5 representative TPC-H queries:

| Query | Type | Parallelism | Tasks | Avg Task Time |
|-------|------|-------------|-------|---------------|
| Q1 | ELASTIC | 8 | 80 | 50ms |
| Q6 | ELASTIC | 8 | 40 | 25ms |
| Q18 | INELASTIC | 4 | 60 | 80ms |
| Q3 | ELASTIC | 8 | 70 | 60ms |
| Q10 | INELASTIC | 4 | 50 | 70ms |

## Experiments

### Phase 0 Experiments

1. **Single ELASTIC Query** - Q1 performance baseline
2. **Single INELASTIC Query** - Q18 performance baseline
3. **Mixed Workload** - All 5 queries together
4. **ELASTIC-Only Workload** - Q1, Q6, Q3
5. **INELASTIC-Only Workload** - Q18, Q10
6. **High Concurrency** - All queries, 10 runs for statistical significance

Each experiment runs 5 times (except high concurrency which runs 10 times) to ensure statistical validity.

## Usage

The benchmark framework is integrated into DuckDB's test suite. To run the tests:

```bash
cd build/debug
./test/unittest "[phase0]"
```

## Implementation Notes

- The framework simulates query execution rather than running actual SQL queries
- Task execution times follow a normal distribution with 10% standard deviation
- ELASTIC queries are perfectly parallelizable
- INELASTIC queries have 25% serial portion
- Scheduling overhead is simulated as 1-5% of execution time
- Default morsel size is 1024 tuples

## Future Work

- Phase 1-5 data generation programs
- Integration with actual DuckDB query execution
- Real TPC-H benchmark integration
- Performance comparison with native DuckDB scheduler
