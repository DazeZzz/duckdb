#include "duckdb/parallel/benchmark_framework.hpp"
#include <iostream>

using duckdb::BenchmarkFramework;
using duckdb::TPCHQuery;
using duckdb::TaskPhase;
using duckdb::AggregatedMetrics;
using duckdb::vector;

int main() {
	std::cout << "=== Phase 0: Baseline Experimental Data Generation ===\n";
	std::cout << "This generates baseline performance data without any optimizations.\n";
	std::cout << "\n";

	// Create benchmark framework with 8 threads (typical configuration)
	BenchmarkFramework benchmark(8);

	// Get TPC-H query mix
	auto queries = BenchmarkFramework::GetTPCHMix();

	std::cout << "Running experiments with " << queries.size() << " TPC-H queries...\n";
	std::cout << "Each experiment will run 5 times for statistical significance.\n";
	std::cout << "\n";

	// Collect results for all experiments
	vector<AggregatedMetrics> all_results;

	// Experiment 1: Single query performance (ELASTIC)
	std::cout << "[1/6] Running single ELASTIC query (Q1)...\n";
	vector<TPCHQuery> q1_only;
	q1_only.push_back(queries[0]); // Q1
	auto result1 = benchmark.RunExperiment("Phase0-Single-ELASTIC-Q1", q1_only, 5);
	all_results.push_back(result1);
	std::cout << "  Avg response time: " << result1.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result1.avg_throughput << " tasks/sec\n";
	std::cout << "\n";

	// Experiment 2: Single query performance (INELASTIC)
	std::cout << "[2/6] Running single INELASTIC query (Q18)...\n";
	vector<TPCHQuery> q18_only;
	q18_only.push_back(queries[2]); // Q18
	auto result2 = benchmark.RunExperiment("Phase0-Single-INELASTIC-Q18", q18_only, 5);
	all_results.push_back(result2);
	std::cout << "  Avg response time: " << result2.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result2.avg_throughput << " tasks/sec\n";
	std::cout << "\n";

	// Experiment 3: Mixed workload (all queries)
	std::cout << "[3/6] Running mixed workload (all 5 queries)...\n";
	auto result3 = benchmark.RunExperiment("Phase0-Mixed-Workload", queries, 5);
	all_results.push_back(result3);
	std::cout << "  Avg response time: " << result3.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result3.avg_throughput << " tasks/sec\n";
	std::cout << "\n";

	// Experiment 4: ELASTIC-only workload
	std::cout << "[4/6] Running ELASTIC-only workload (Q1, Q6, Q3)...\n";
	vector<TPCHQuery> elastic_queries;
	elastic_queries.push_back(queries[0]); // Q1
	elastic_queries.push_back(queries[1]); // Q6
	elastic_queries.push_back(queries[3]); // Q3
	auto result4 = benchmark.RunExperiment("Phase0-ELASTIC-Only", elastic_queries, 5);
	all_results.push_back(result4);
	std::cout << "  Avg response time: " << result4.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result4.avg_throughput << " tasks/sec\n";
	std::cout << "\n";

	// Experiment 5: INELASTIC-only workload
	std::cout << "[5/6] Running INELASTIC-only workload (Q18, Q10)...\n";
	vector<TPCHQuery> inelastic_queries;
	inelastic_queries.push_back(queries[2]); // Q18
	inelastic_queries.push_back(queries[4]); // Q10
	auto result5 = benchmark.RunExperiment("Phase0-INELASTIC-Only", inelastic_queries, 5);
	all_results.push_back(result5);
	std::cout << "  Avg response time: " << result5.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result5.avg_throughput << " tasks/sec\n";
	std::cout << "\n";

	// Experiment 6: High concurrency (all queries, 10 runs)
	std::cout << "[6/6] Running high concurrency test (all queries, 10 runs)...\n";
	auto result6 = benchmark.RunExperiment("Phase0-High-Concurrency", queries, 10);
	all_results.push_back(result6);
	std::cout << "  Avg response time: " << result6.avg_response_time_ms << " ms\n";
	std::cout << "  Avg throughput: " << result6.avg_throughput << " tasks/sec\n";
	std::cout << "  P50 latency: " << result6.p50_latency_ms << " ms\n";
	std::cout << "  P90 latency: " << result6.p90_latency_ms << " ms\n";
	std::cout << "  P99 latency: " << result6.p99_latency_ms << " ms\n";
	std::cout << "\n";

	// Export results to CSV
	std::cout << "Exporting aggregated results to CSV...\n";
	benchmark.ExportToCSV("phase0_results.csv", all_results);
	std::cout << "  Saved to: phase0_results.csv\n";

	// Export detailed logs for the mixed workload
	std::cout << "Exporting detailed logs to JSON...\n";
	auto detailed_metrics = benchmark.RunWorkload(queries);
	benchmark.ExportDetailedLogs("phase0_detailed_logs.json", detailed_metrics);
	std::cout << "  Saved to: phase0_detailed_logs.json\n";

	std::cout << "\n";
	std::cout << "=== Phase 0 Data Generation Complete ===\n";
	std::cout << "Generated files:\n";
	std::cout << "  - phase0_results.csv (aggregated metrics)\n";
	std::cout << "  - phase0_detailed_logs.json (detailed execution logs)\n";

	return 0;
}
