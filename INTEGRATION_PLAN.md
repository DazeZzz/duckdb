# Phase-Aware Scheduler Integration Plan

## Executive Summary

The phase-aware scheduler (Phase 0-5) has been **fully implemented** but is **not currently active** in DuckDB's execution engine. The infrastructure exists but `use_priority_scheduling` is hardcoded to `false` in task_scheduler.cpp:231.

This document outlines the integration plan to enable the phase-aware scheduler and generate experimental data for the academic paper.

## Current Implementation Status

### ✅ Completed (Phase 0-5)

**Phase 1: Phase-Aware Scheduling**
- ✅ TaskPhase enum (ELASTIC vs INELASTIC)
- ✅ ResourceGroup with phase tracking
- ✅ PriorityTaskQueue with Inelastic-First (IF) strategy
- ✅ Stride scheduling algorithm
- ✅ Tests: test_priority_scheduling.cpp

**Phase 2: Adaptive Morsel Execution**
- ✅ ExecutionState enum (Startup/Default/Shutdown)
- ✅ Dynamic morsel size (10/50/10 chunks)
- ✅ Throughput estimation with EMA
- ✅ Photo finish mechanism
- ✅ Pipeline class modifications
- ✅ Tests: test_adaptive_morsel.cpp

**Phase 3: Adaptive Priorities**
- ✅ ResourceGroup throughput tracking
- ✅ AdaptPriority() based on relative throughput
- ✅ Priority bounds (0.25x - 4.0x)
- ✅ PriorityTaskQueue adaptive priority support

**Phase 4: Lock-Free Scheduler**
- ✅ lock_free_priority_task_queue.hpp/cpp
- ✅ Lock-free implementation

**Phase 5: Self-Tuning Configuration**
- ✅ SelfTuningConfig class
- ✅ Performance metrics tracking
- ✅ AutoTune() method
- ✅ Tunable parameters
- ✅ Tests: test_self_tuning.cpp

### ❌ Missing: Integration with Execution Engine

**Critical Issue**: `use_priority_scheduling = false` (task_scheduler.cpp:231)

The phase-aware scheduler is **dormant**. All queries currently use the legacy FIFO queue.

## Integration Plan

### Step 1: Enable Phase-Aware Scheduler (Priority: HIGH)

**Goal**: Make the phase-aware scheduler active during query execution.

**Tasks**:
1. Add database configuration option `enable_phase_aware_scheduling`
2. Modify TaskScheduler constructor to read this config
3. Set default to `true` for experimental branch
4. Add runtime toggle: `SET enable_phase_aware_scheduling = true/false`

**Files to modify**:
- `src/main/config.cpp` - add configuration option
- `src/parallel/task_scheduler.cpp` - read config instead of hardcoded false
- `src/include/duckdb/main/config.hpp` - declare config option

**Estimated effort**: 2-3 hours

### Step 2: Set TaskPhase Correctly (Priority: HIGH)

**Goal**: Classify pipeline operators as ELASTIC or INELASTIC and set phases during execution.

**Background**:
- **ELASTIC**: Perfectly parallelizable (scan, filter, project, hash probe, join probe)
- **INELASTIC**: Serial or limited parallelism (hash build, aggregate finalize, sort, top-n)

**Tasks**:
1. Analyze DuckDB's physical operators to classify them
2. Add phase detection logic in Pipeline or PipelineExecutor
3. Set ResourceGroup phase when creating/scheduling tasks
4. Handle phase transitions during query execution

**Files to modify**:
- `src/parallel/pipeline_executor.cpp` - detect operator phase
- `src/parallel/pipeline.cpp` - set phase on resource group
- `src/execution/physical_plan/*.cpp` - annotate operators with phase info (if needed)

**Operator Classification** (preliminary):
```
ELASTIC:
- PhysicalTableScan
- PhysicalFilter
- PhysicalProjection
- PhysicalHashJoin (probe side)
- PhysicalNestedLoopJoin (probe side)

INELASTIC:
- PhysicalHashAggregate (finalize)
- PhysicalOrder (sort)
- PhysicalTopN
- PhysicalHashJoin (build side)
- PhysicalWindow (partition/order)
```

**Estimated effort**: 4-6 hours

### Step 3: Integrate Performance Monitoring (Priority: MEDIUM)

**Goal**: Collect real performance metrics during query execution for self-tuning and experimental data.

**Tasks**:
1. Integrate SelfTuningConfig with TaskScheduler
2. Collect metrics: query latency, throughput, CPU utilization, task execution time
3. Update metrics after each task execution
4. Trigger AutoTune() periodically
5. Add logging infrastructure for experimental data collection

**Files to modify**:
- `src/parallel/task_scheduler.cpp` - integrate SelfTuningConfig
- `src/parallel/executor.cpp` - collect query-level metrics
- `src/parallel/pipeline_executor.cpp` - collect task-level metrics

**Metrics to collect**:
- Query response time (end-to-end)
- Task execution time (per-task)
- Scheduling overhead (time spent in scheduler)
- Throughput (queries/sec, tasks/sec)
- CPU utilization (active threads / total threads)
- Phase distribution (% time in ELASTIC vs INELASTIC)

**Estimated effort**: 3-4 hours

### Step 4: Add Experimental Data Export (Priority: HIGH)

**Goal**: Export performance data in formats suitable for paper analysis.

**Tasks**:
1. Create PerformanceLogger class to collect metrics
2. Add hooks to log scheduling decisions
3. Export data to CSV/JSON for analysis
4. Integrate with existing BenchmarkFramework

**Files to create/modify**:
- `src/parallel/performance_logger.hpp/cpp` - new logging infrastructure
- `src/parallel/task_scheduler.cpp` - log scheduling decisions
- `test/api/generate_phase0_data.cpp` - use real queries instead of simulation

**Data to export**:
- Per-query metrics: response_time, throughput, phase_distribution
- Per-task metrics: execution_time, wait_time, phase, priority
- Scheduler metrics: scheduling_overhead, context_switches, pass_values
- System metrics: CPU_utilization, active_threads, queue_length

**Estimated effort**: 3-4 hours

### Step 5: Validation and Testing (Priority: HIGH)

**Goal**: Ensure correctness and backward compatibility.

**Tasks**:
1. Run all existing tests with phase-aware scheduler enabled
2. Verify results match baseline (correctness)
3. Add integration tests with real TPC-H queries
4. Compare performance: phase-aware ON vs OFF
5. Debug any issues or regressions

**Files to create/modify**:
- `test/api/test_phase_aware_integration.cpp` - new integration tests
- Run existing test suite: `make test`

**Test scenarios**:
- Single query execution (ELASTIC only, INELASTIC only, mixed)
- Concurrent queries (2, 4, 8 queries)
- TPC-H queries (Q1, Q3, Q6, Q10, Q18)
- Priority scheduling (high vs low priority queries)
- Adaptive priorities (verify priority adjustments)
- Self-tuning (verify parameter adjustments)

**Estimated effort**: 4-6 hours

### Step 6: Generate Experimental Data (Priority: HIGH)

**Goal**: Run experiments specified in EXPERIMENT_REQUIREMENTS.md and collect data for the paper.

**Experiments** (from EXPERIMENT_REQUIREMENTS.md):
1. **Phase 0 Baseline**: Measure baseline performance without phase-aware scheduling
2. **Phase 1 Evaluation**: Compare IF strategy vs baseline
3. **Phase 2 Evaluation**: Measure adaptive morsel execution benefits
4. **Phase 3 Evaluation**: Evaluate adaptive priorities
5. **Phase 4 Evaluation**: Compare lock-free vs lock-based scheduler
6. **Phase 5 Evaluation**: Measure self-tuning effectiveness

**Tasks**:
1. Set up TPC-H database (scale factor 1)
2. Run each experiment with multiple repetitions (10-20 runs)
3. Collect metrics for each configuration
4. Export data to CSV/JSON
5. Generate summary statistics and plots

**Files to create/modify**:
- `test/api/generate_phase0_data.cpp` - update to use real queries
- `test/api/generate_phase1_data.cpp` - Phase 1 experiments
- `test/api/generate_phase2_data.cpp` - Phase 2 experiments
- `test/api/generate_phase3_data.cpp` - Phase 3 experiments
- `test/api/generate_phase4_data.cpp` - Phase 4 experiments
- `test/api/generate_phase5_data.cpp` - Phase 5 experiments

**Estimated effort**: 6-8 hours (including experiment runtime)

## Implementation Timeline

### Week 1: Core Integration
- **Day 1-2**: Step 1 (Enable scheduler) + Step 2 (Set phases)
- **Day 3**: Step 3 (Performance monitoring)
- **Day 4**: Step 4 (Data export)
- **Day 5**: Step 5 (Validation)

### Week 2: Experimental Data Generation
- **Day 1-2**: Set up experiments and run Phase 0-2
- **Day 3-4**: Run Phase 3-5 experiments
- **Day 5**: Data analysis and validation

**Total estimated effort**: 22-31 hours over 2 weeks

## Risk Assessment

### High Risk
- **Operator phase classification**: May be complex to determine phase for all operators
  - *Mitigation*: Start with simple heuristics, refine based on profiling

- **Performance regression**: Phase-aware scheduler might be slower than FIFO for some workloads
  - *Mitigation*: Keep `use_priority_scheduling` configurable, extensive testing

### Medium Risk
- **Concurrency bugs**: Phase-aware scheduler has more complex locking
  - *Mitigation*: Use lock-free implementation (Phase 4), stress testing

- **Metric collection overhead**: Logging might impact performance
  - *Mitigation*: Make logging optional, use lock-free counters

### Low Risk
- **Test failures**: Existing tests might fail with new scheduler
  - *Mitigation*: Fix bugs, ensure backward compatibility

## Success Criteria

1. ✅ Phase-aware scheduler is active and used during query execution
2. ✅ All existing tests pass with phase-aware scheduler enabled
3. ✅ TaskPhase is correctly set for all pipeline operators
4. ✅ Performance metrics are collected and exported
5. ✅ Experimental data matches requirements in EXPERIMENT_REQUIREMENTS.md
6. ✅ Performance improvements are measurable and reproducible
7. ✅ Data is suitable for academic paper publication

## Next Steps

1. **Immediate**: Start with Step 1 (Enable scheduler)
2. **Short-term**: Complete Steps 2-4 (Integration)
3. **Medium-term**: Complete Step 5 (Validation)
4. **Long-term**: Complete Step 6 (Experimental data)

## References

- Berg et al. "The case for phase-aware scheduling of parallelizable jobs" (Performance Evaluation 2021)
- Wagner et al. "Self-Tuning Query Scheduling" (SIGMOD 2021)
- EXPERIMENT_REQUIREMENTS.md - Detailed experimental requirements
- Phase 1-5 implementation commits (7e5eb201ad - 7777a4c47c)
