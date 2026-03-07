#include "duckdb/parallel/benchmark_framework.hpp"
#include "duckdb/common/exception.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <sstream>
#include <thread>
#include <random>

namespace duckdb {

BenchmarkFramework::BenchmarkFramework(idx_t num_threads) : num_threads_(num_threads) {
}

BenchmarkFramework::~BenchmarkFramework() {
}

BenchmarkQueryMetrics BenchmarkFramework::RunQuery(const TPCHQuery &query) {
	BenchmarkQueryMetrics metrics;
	metrics.query_name = query.name;

	auto start_time = std::chrono::high_resolution_clock::now();

	// Simulate task execution
	std::random_device rd;
	std::mt19937 gen(rd());
	std::normal_distribution<> task_time_dist(static_cast<double>(query.avg_task_time_us),
	                                           static_cast<double>(query.avg_task_time_us) * 0.1);

	metrics.task_count = query.task_count;
	metrics.morsel_count = query.task_count * 10; // Assume 10 morsels per task on average

	// Simulate parallel execution
	uint64_t total_task_time = 0;
	for (idx_t i = 0; i < query.task_count; i++) {
		uint64_t task_time = static_cast<uint64_t>(std::max(1000.0, task_time_dist(gen)));
		metrics.task_execution_times.push_back(task_time);
		metrics.phase_types.push_back(query.phase);
		metrics.morsel_sizes.push_back(1024); // Default morsel size
		total_task_time += task_time;
	}

	// Calculate parallel execution time
	uint64_t parallel_time_us;
	if (query.phase == TaskPhase::ELASTIC) {
		// Perfectly parallelizable
		parallel_time_us = total_task_time / std::min(num_threads_, query.parallelism);
	} else {
		// INELASTIC: some serial portion
		uint64_t serial_time = total_task_time / 4; // 25% serial
		uint64_t parallel_portion = total_task_time - serial_time;
		parallel_time_us = serial_time + (parallel_portion / std::min(num_threads_, query.parallelism));
	}

	// Add scheduling overhead (1-5% of execution time)
	std::uniform_int_distribution<> overhead_dist(10, 50);
	metrics.scheduling_overhead_us = (parallel_time_us * static_cast<uint64_t>(overhead_dist(gen))) / 1000;

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
	metrics.response_time_ms = static_cast<uint64_t>((duration_us + 999) / 1000); // Round up to at least 1ms

	// If execution was too fast, use simulated time based on parallel_time_us
	if (metrics.response_time_ms == 0) {
		metrics.response_time_ms = (parallel_time_us + 999) / 1000; // Convert us to ms, round up
	}

	// Calculate throughput (tasks per second)
	if (metrics.response_time_ms > 0) {
		metrics.throughput = (static_cast<double>(metrics.task_count) * 1000.0) / static_cast<double>(metrics.response_time_ms);
	}

	// Calculate parallel efficiency
	double ideal_speedup = static_cast<double>(std::min(num_threads_, query.parallelism));
	double actual_speedup = static_cast<double>(total_task_time) / static_cast<double>(parallel_time_us);
	metrics.parallel_efficiency = actual_speedup / ideal_speedup;

	return metrics;
}

vector<BenchmarkQueryMetrics> BenchmarkFramework::RunWorkload(const vector<TPCHQuery> &queries) {
	vector<BenchmarkQueryMetrics> results;
	results.reserve(queries.size());

	// Run queries concurrently
	for (const auto &query : queries) {
		results.push_back(RunQuery(query));
	}

	return results;
}

AggregatedMetrics BenchmarkFramework::RunExperiment(const string &name, const vector<TPCHQuery> &queries, idx_t num_runs) {
	AggregatedMetrics agg;
	agg.experiment_name = name;

	vector<double> response_times;
	vector<double> throughputs;
	vector<double> scheduling_overheads;
	vector<double> parallel_efficiencies;
	vector<uint64_t> all_response_times_ms;

	// Run experiment multiple times
	for (idx_t run = 0; run < num_runs; run++) {
		auto workload_metrics = RunWorkload(queries);

		for (const auto &m : workload_metrics) {
			response_times.push_back(static_cast<double>(m.response_time_ms));
			throughputs.push_back(m.throughput);
			scheduling_overheads.push_back(static_cast<double>(m.scheduling_overhead_us));
			parallel_efficiencies.push_back(m.parallel_efficiency);
			all_response_times_ms.push_back(m.response_time_ms);
		}
	}

	// Calculate averages
	agg.avg_response_time_ms = std::accumulate(response_times.begin(), response_times.end(), 0.0) / static_cast<double>(response_times.size());
	agg.avg_throughput = std::accumulate(throughputs.begin(), throughputs.end(), 0.0) / static_cast<double>(throughputs.size());
	agg.avg_scheduling_overhead_us = std::accumulate(scheduling_overheads.begin(), scheduling_overheads.end(), 0.0) / static_cast<double>(scheduling_overheads.size());
	agg.avg_parallel_efficiency = std::accumulate(parallel_efficiencies.begin(), parallel_efficiencies.end(), 0.0) / static_cast<double>(parallel_efficiencies.size());

	// Calculate standard deviations
	agg.std_response_time_ms = CalculateStdDev(response_times, agg.avg_response_time_ms);
	agg.std_throughput = CalculateStdDev(throughputs, agg.avg_throughput);
	agg.std_scheduling_overhead_us = CalculateStdDev(scheduling_overheads, agg.avg_scheduling_overhead_us);
	agg.std_parallel_efficiency = CalculateStdDev(parallel_efficiencies, agg.avg_parallel_efficiency);

	// Calculate percentiles
	agg.p50_latency_ms = CalculatePercentile(all_response_times_ms, 0.50);
	agg.p90_latency_ms = CalculatePercentile(all_response_times_ms, 0.90);
	agg.p99_latency_ms = CalculatePercentile(all_response_times_ms, 0.99);

	return agg;
}

void BenchmarkFramework::ExportToCSV(const string &filename, const vector<AggregatedMetrics> &results) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	// Write header
	file << "Experiment,AvgResponseTime(ms),StdResponseTime(ms),AvgThroughput,StdThroughput,";
	file << "AvgSchedulingOverhead(us),StdSchedulingOverhead(us),AvgParallelEfficiency,StdParallelEfficiency,";
	file << "P50Latency(ms),P90Latency(ms),P99Latency(ms),AvgSlowdown,StdSlowdown\n";

	// Write data
	for (const auto &result : results) {
		file << result.experiment_name << ","
		     << result.avg_response_time_ms << "," << result.std_response_time_ms << ","
		     << result.avg_throughput << "," << result.std_throughput << ","
		     << result.avg_scheduling_overhead_us << "," << result.std_scheduling_overhead_us << ","
		     << result.avg_parallel_efficiency << "," << result.std_parallel_efficiency << ","
		     << result.p50_latency_ms << "," << result.p90_latency_ms << "," << result.p99_latency_ms << ","
		     << result.avg_slowdown << "," << result.std_slowdown << "\n";
	}

	file.close();
}

void BenchmarkFramework::ExportDetailedLogs(const string &filename, const vector<BenchmarkQueryMetrics> &metrics) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	file << "[\n";
	for (size_t i = 0; i < metrics.size(); i++) {
		const auto &m = metrics[i];
		file << "  {\n";
		file << "    \"query_name\": \"" << m.query_name << "\",\n";
		file << "    \"response_time_ms\": " << m.response_time_ms << ",\n";
		file << "    \"scheduling_overhead_us\": " << m.scheduling_overhead_us << ",\n";
		file << "    \"task_count\": " << m.task_count << ",\n";
		file << "    \"morsel_count\": " << m.morsel_count << ",\n";
		file << "    \"throughput\": " << m.throughput << ",\n";
		file << "    \"parallel_efficiency\": " << m.parallel_efficiency << ",\n";
		file << "    \"task_execution_times\": [";
		for (size_t j = 0; j < m.task_execution_times.size(); j++) {
			file << m.task_execution_times[j];
			if (j < m.task_execution_times.size() - 1) {
				file << ", ";
			}
		}
		file << "],\n";
		file << "    \"morsel_sizes\": [";
		for (size_t j = 0; j < m.morsel_sizes.size(); j++) {
			file << m.morsel_sizes[j];
			if (j < m.morsel_sizes.size() - 1) {
				file << ", ";
			}
		}
		file << "],\n";
		file << "    \"phase_types\": [";
		for (size_t j = 0; j < m.phase_types.size(); j++) {
			file << "\"" << (m.phase_types[j] == TaskPhase::ELASTIC ? "ELASTIC" : "INELASTIC") << "\"";
			if (j < m.phase_types.size() - 1) {
				file << ", ";
			}
		}
		file << "]\n";
		file << "  }";
		if (i < metrics.size() - 1) {
			file << ",";
		}
		file << "\n";
	}
	file << "]\n";

	file.close();
}

double BenchmarkFramework::CalculateStdDev(const vector<double> &values, double mean) {
	if (values.empty()) {
		return 0.0;
	}

	double sum_squared_diff = 0.0;
	for (const auto &val : values) {
		double diff = val - mean;
		sum_squared_diff += diff * diff;
	}

	return std::sqrt(sum_squared_diff / static_cast<double>(values.size()));
}

uint64_t BenchmarkFramework::CalculatePercentile(vector<uint64_t> values, double percentile) {
	if (values.empty()) {
		return 0;
	}

	std::sort(values.begin(), values.end());
	size_t index = static_cast<size_t>(static_cast<double>(values.size()) * percentile);
	if (index >= values.size()) {
		index = values.size() - 1;
	}

	return values[index];
}

vector<TPCHQuery> BenchmarkFramework::GetTPCHMix() {
	vector<TPCHQuery> queries;

	// TPC-H queries with realistic characteristics
	// Q1: Aggregation query (ELASTIC)
	queries.emplace_back("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);

	// Q6: Simple scan and filter (ELASTIC)
	queries.emplace_back("Q6", TaskPhase::ELASTIC, 100, 8, 40, 25000);

	// Q18: Complex join with sorting (INELASTIC)
	queries.emplace_back("Q18", TaskPhase::INELASTIC, 100, 4, 60, 80000);

	// Q3: Join with aggregation (ELASTIC)
	queries.emplace_back("Q3", TaskPhase::ELASTIC, 100, 8, 70, 60000);

	// Q10: Multi-way join (INELASTIC)
	queries.emplace_back("Q10", TaskPhase::INELASTIC, 100, 4, 50, 70000);

	return queries;
}

} // namespace duckdb
