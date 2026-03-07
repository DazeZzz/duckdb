//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/benchmark_framework.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/task_phase.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <map>

namespace duckdb {

// Performance metrics for a single query execution
struct BenchmarkQueryMetrics {
	string query_name;
	uint64_t response_time_ms;           // Total execution time
	uint64_t scheduling_overhead_us;     // Time spent in scheduling
	uint64_t task_count;                 // Number of tasks executed
	uint64_t morsel_count;               // Number of morsels processed
	double throughput;                   // Tasks per second
	double parallel_efficiency;          // Actual speedup / ideal speedup
	vector<uint64_t> task_execution_times; // Individual task times (us)
	vector<idx_t> morsel_sizes;          // Morsel sizes used
	vector<TaskPhase> phase_types;       // ELASTIC or INELASTIC
	vector<uint64_t> scheduling_events;  // Timestamps of scheduling decisions

	// Constructor
	BenchmarkQueryMetrics() : response_time_ms(0), scheduling_overhead_us(0),
	                 task_count(0), morsel_count(0), throughput(0.0),
	                 parallel_efficiency(0.0) {}
};

// Aggregated metrics across multiple runs
struct AggregatedMetrics {
	string experiment_name;
	double avg_response_time_ms;
	double std_response_time_ms;
	double avg_throughput;
	double std_throughput;
	double avg_scheduling_overhead_us;
	double std_scheduling_overhead_us;
	double avg_parallel_efficiency;
	double std_parallel_efficiency;

	// Latency percentiles
	uint64_t p50_latency_ms;
	uint64_t p90_latency_ms;
	uint64_t p99_latency_ms;

	// Slowdown (compared to baseline)
	double avg_slowdown;
	double std_slowdown;

	// Constructor
	AggregatedMetrics() : avg_response_time_ms(0.0), std_response_time_ms(0.0),
	                      avg_throughput(0.0), std_throughput(0.0),
	                      avg_scheduling_overhead_us(0.0), std_scheduling_overhead_us(0.0),
	                      avg_parallel_efficiency(0.0), std_parallel_efficiency(0.0),
	                      p50_latency_ms(0), p90_latency_ms(0), p99_latency_ms(0),
	                      avg_slowdown(1.0), std_slowdown(0.0) {}
};

// TPC-H query characteristics
struct TPCHQuery {
	string name;              // e.g., "Q1", "Q6", "Q18"
	TaskPhase phase;          // ELASTIC or INELASTIC
	uint64_t base_priority;   // Base priority for the query
	idx_t parallelism;        // Degree of parallelism
	idx_t task_count;         // Number of tasks
	uint64_t avg_task_time_us; // Average task execution time

	TPCHQuery(const string &n, TaskPhase p, uint64_t prio, idx_t par, idx_t tasks, uint64_t time)
	    : name(n), phase(p), base_priority(prio), parallelism(par), task_count(tasks), avg_task_time_us(time) {}
};

// Benchmark framework for generating experimental data
class BenchmarkFramework {
public:
	explicit BenchmarkFramework(idx_t num_threads);
	~BenchmarkFramework();

	// Run a single query and collect metrics
	BenchmarkQueryMetrics RunQuery(const TPCHQuery &query);

	// Run multiple queries concurrently (workload mix)
	vector<BenchmarkQueryMetrics> RunWorkload(const vector<TPCHQuery> &queries);

	// Run experiment multiple times and aggregate results
	AggregatedMetrics RunExperiment(const string &name, const vector<TPCHQuery> &queries, idx_t num_runs);

	// Export results to CSV
	void ExportToCSV(const string &filename, const vector<AggregatedMetrics> &results);

	// Export detailed logs to JSON
	void ExportDetailedLogs(const string &filename, const vector<BenchmarkQueryMetrics> &metrics);

	// Get predefined TPC-H query mix
	static vector<TPCHQuery> GetTPCHMix();

private:
	idx_t num_threads_;
	mutex metrics_lock_;

	// Helper functions
	double CalculateStdDev(const vector<double> &values, double mean);
	uint64_t CalculatePercentile(vector<uint64_t> values, double percentile);
};

} // namespace duckdb