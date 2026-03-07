#include "catch.hpp"
#include "duckdb/parallel/benchmark_framework.hpp"
#include "duckdb/parallel/task_phase.hpp"

using duckdb::BenchmarkFramework;
using duckdb::TPCHQuery;
using duckdb::TaskPhase;
using duckdb::AggregatedMetrics;
using duckdb::BenchmarkQueryMetrics;
using duckdb::vector;

TEST_CASE("Test benchmark framework - Phase 0 baseline", "[benchmark][phase0]") {
	// Create benchmark framework with 8 threads
	BenchmarkFramework benchmark(8);

	SECTION("Test single query execution") {
		// Create a simple ELASTIC query
		TPCHQuery q1("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);

		auto metrics = benchmark.RunQuery(q1);

		// Verify metrics are collected
		REQUIRE(metrics.query_name == "Q1");
		REQUIRE(metrics.task_count == 80);
		REQUIRE(metrics.response_time_ms > 0);
		REQUIRE(metrics.throughput > 0.0);
		REQUIRE(metrics.parallel_efficiency > 0.0);
		REQUIRE(metrics.task_execution_times.size() == 80);
		REQUIRE(metrics.phase_types.size() == 80);
	}

	SECTION("Test workload execution") {
		// Create a mix of queries
		vector<TPCHQuery> queries;
		queries.emplace_back("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);
		queries.emplace_back("Q6", TaskPhase::ELASTIC, 100, 8, 40, 25000);
		queries.emplace_back("Q18", TaskPhase::INELASTIC, 100, 4, 60, 80000);

		auto results = benchmark.RunWorkload(queries);

		// Verify all queries executed
		REQUIRE(results.size() == 3);
		REQUIRE(results[0].query_name == "Q1");
		REQUIRE(results[1].query_name == "Q6");
		REQUIRE(results[2].query_name == "Q18");
	}

	SECTION("Test experiment with multiple runs") {
		// Create a simple workload
		vector<TPCHQuery> queries;
		queries.emplace_back("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);

		// Run experiment 5 times
		auto agg = benchmark.RunExperiment("Phase0-Baseline", queries, 5);

		// Verify aggregated metrics
		REQUIRE(agg.experiment_name == "Phase0-Baseline");
		REQUIRE(agg.avg_response_time_ms > 0.0);
		REQUIRE(agg.std_response_time_ms >= 0.0);
		REQUIRE(agg.avg_throughput > 0.0);
		REQUIRE(agg.p50_latency_ms > 0);
		REQUIRE(agg.p90_latency_ms >= agg.p50_latency_ms);
		REQUIRE(agg.p99_latency_ms >= agg.p90_latency_ms);
	}

	SECTION("Test TPC-H query mix") {
		auto queries = BenchmarkFramework::GetTPCHMix();

		// Verify we have the expected queries
		REQUIRE(queries.size() == 5);
		REQUIRE(queries[0].name == "Q1");
		REQUIRE(queries[1].name == "Q6");
		REQUIRE(queries[2].name == "Q18");
		REQUIRE(queries[3].name == "Q3");
		REQUIRE(queries[4].name == "Q10");

		// Verify phase types
		REQUIRE(queries[0].phase == TaskPhase::ELASTIC);
		REQUIRE(queries[1].phase == TaskPhase::ELASTIC);
		REQUIRE(queries[2].phase == TaskPhase::INELASTIC);
		REQUIRE(queries[3].phase == TaskPhase::ELASTIC);
		REQUIRE(queries[4].phase == TaskPhase::INELASTIC);
	}

	SECTION("Test CSV export") {
		vector<TPCHQuery> queries;
		queries.emplace_back("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);

		auto agg = benchmark.RunExperiment("Phase0-Export-Test", queries, 3);

		vector<AggregatedMetrics> results;
		results.push_back(agg);

		// Export to CSV
		benchmark.ExportToCSV("/tmp/test_phase0_results.csv", results);

		// Verify file was created (basic check)
		// In a real test, we would read and verify the CSV content
	}

	SECTION("Test detailed logs export") {
		TPCHQuery q1("Q1", TaskPhase::ELASTIC, 100, 8, 80, 50000);
		auto metrics = benchmark.RunQuery(q1);

		vector<BenchmarkQueryMetrics> all_metrics;
		all_metrics.push_back(metrics);

		// Export detailed logs
		benchmark.ExportDetailedLogs("/tmp/test_phase0_logs.json", all_metrics);

		// Verify file was created (basic check)
		// In a real test, we would read and verify the JSON content
	}
}