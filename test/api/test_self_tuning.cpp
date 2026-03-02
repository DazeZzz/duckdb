#include "catch.hpp"
#include "duckdb/parallel/self_tuning_config.hpp"
#include "test_helpers.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test self-tuning config initialization", "[self_tuning]") {
	SelfTuningConfig config;

	// Check default parameters
	auto params = config.GetParameters();
	REQUIRE(params.startup_morsel_size == 10);
	REQUIRE(params.default_morsel_size == 50);
	REQUIRE(params.shutdown_morsel_size == 10);
	REQUIRE(params.startup_threshold == 100);
	REQUIRE(params.shutdown_threshold == Approx(0.9));
	REQUIRE(params.ema_alpha == Approx(0.3));
	REQUIRE(params.min_priority_factor == Approx(0.25));
	REQUIRE(params.max_priority_factor == Approx(4.0));

	// Check auto-tuning is enabled by default
	REQUIRE(config.IsAutoTuningEnabled());
}

TEST_CASE("Test performance metrics update", "[self_tuning]") {
	SelfTuningConfig config;

	// Update metrics
	SelfTuningConfig::PerformanceMetrics metrics;
	metrics.avg_query_latency = 1.5;
	metrics.system_throughput = 100.0;
	metrics.cpu_utilization = 0.75;
	metrics.active_queries = 5;
	metrics.completed_queries = 50;
	metrics.avg_task_execution_time = 0.1;

	config.UpdateMetrics(metrics);

	// Verify metrics are stored
	auto retrieved = config.GetMetrics();
	REQUIRE(retrieved.avg_query_latency == Approx(1.5));
	REQUIRE(retrieved.system_throughput == Approx(100.0));
	REQUIRE(retrieved.cpu_utilization == Approx(0.75));
	REQUIRE(retrieved.active_queries == 5);
	REQUIRE(retrieved.completed_queries == 50);
	REQUIRE(retrieved.avg_task_execution_time == Approx(0.1));
}

TEST_CASE("Test parameter adjustment", "[self_tuning]") {
	SelfTuningConfig config;

	// Set custom parameters
	SelfTuningConfig::TunableParameters params;
	params.default_morsel_size = 100;
	params.ema_alpha = 0.5;
	params.min_priority_factor = 0.5;
	params.max_priority_factor = 2.0;

	config.SetParameters(params);

	// Verify parameters are updated
	auto retrieved = config.GetParameters();
	REQUIRE(retrieved.default_morsel_size == 100);
	REQUIRE(retrieved.ema_alpha == Approx(0.5));
	REQUIRE(retrieved.min_priority_factor == Approx(0.5));
	REQUIRE(retrieved.max_priority_factor == Approx(2.0));
}

TEST_CASE("Test auto-tuning enable/disable", "[self_tuning]") {
	SelfTuningConfig config;

	// Initially enabled
	REQUIRE(config.IsAutoTuningEnabled());

	// Disable
	config.SetAutoTuning(false);
	REQUIRE(!config.IsAutoTuningEnabled());

	// Enable
	config.SetAutoTuning(true);
	REQUIRE(config.IsAutoTuningEnabled());
}

TEST_CASE("Test morsel size auto-adjustment", "[self_tuning]") {
	SelfTuningConfig config;

	// Scenario 1: High CPU utilization + low latency -> increase morsel size
	SelfTuningConfig::PerformanceMetrics metrics1;
	metrics1.cpu_utilization = 0.95;
	metrics1.avg_query_latency = 0.5;
	metrics1.system_throughput = 200.0;

	config.UpdateMetrics(metrics1);
	auto params_before = config.GetParameters();

	config.AutoTune();

	auto params_after = config.GetParameters();
	// Morsel size should increase (or stay same if already at max)
	REQUIRE(params_after.default_morsel_size >= params_before.default_morsel_size);
}

TEST_CASE("Test priority bounds auto-adjustment", "[self_tuning]") {
	SelfTuningConfig config;

	// Scenario: Many active queries -> tighten bounds for fairness
	SelfTuningConfig::PerformanceMetrics metrics;
	metrics.active_queries = 15;
	metrics.system_throughput = 100.0;

	config.UpdateMetrics(metrics);
	auto params_before = config.GetParameters();

	config.AutoTune();

	auto params_after = config.GetParameters();
	// Bounds should tighten (min increases, max decreases)
	REQUIRE(params_after.min_priority_factor >= params_before.min_priority_factor);
	REQUIRE(params_after.max_priority_factor <= params_before.max_priority_factor);
}

TEST_CASE("Test EMA alpha auto-adjustment", "[self_tuning]") {
	SelfTuningConfig config;

	// Create stable throughput pattern (low variance)
	for (int i = 0; i < 10; i++) {
		SelfTuningConfig::PerformanceMetrics metrics;
		metrics.system_throughput = 100.0 + (i % 2); // Very stable
		metrics.cpu_utilization = 0.5;
		config.UpdateMetrics(metrics);
	}

	auto params_before = config.GetParameters();
	config.AutoTune();
	auto params_after = config.GetParameters();

	// Alpha should decrease for stable workload (or stay same)
	REQUIRE(params_after.ema_alpha <= params_before.ema_alpha);
}

TEST_CASE("Test reset to defaults", "[self_tuning]") {
	SelfTuningConfig config;

	// Modify parameters
	SelfTuningConfig::TunableParameters params;
	params.default_morsel_size = 200;
	params.ema_alpha = 0.8;
	config.SetParameters(params);

	// Reset
	config.ResetToDefaults();

	// Verify back to defaults
	auto retrieved = config.GetParameters();
	REQUIRE(retrieved.default_morsel_size == 50);
	REQUIRE(retrieved.ema_alpha == Approx(0.3));
}

TEST_CASE("Test comprehensive self-tuning scenario", "[self_tuning]") {
	SelfTuningConfig config;

	// Simulate a workload evolution
	// Phase 1: Light load
	for (int i = 0; i < 3; i++) {
		SelfTuningConfig::PerformanceMetrics metrics;
		metrics.active_queries = 2;
		metrics.cpu_utilization = 0.3;
		metrics.avg_query_latency = 0.5;
		metrics.system_throughput = 50.0;
		config.UpdateMetrics(metrics);
	}

	auto params_light = config.GetParameters();
	config.AutoTune();
	auto params_after_light = config.GetParameters();

	// Phase 2: Heavy load
	for (int i = 0; i < 3; i++) {
		SelfTuningConfig::PerformanceMetrics metrics;
		metrics.active_queries = 20;
		metrics.cpu_utilization = 0.95;
		metrics.avg_query_latency = 3.0;
		metrics.system_throughput = 150.0;
		config.UpdateMetrics(metrics);
	}

	config.AutoTune();
	auto params_after_heavy = config.GetParameters();

	// Verify system adapted to workload changes
	// Under heavy load, priority bounds should be tighter
	REQUIRE(params_after_heavy.min_priority_factor >= params_after_light.min_priority_factor);
}

TEST_CASE("Test self-tuning with realistic workload", "[self_tuning]") {
	DuckDB db(nullptr);
	Connection con(db);

	SelfTuningConfig config;

	// Create test tables
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test_table AS SELECT range AS id FROM range(10000)"));

	// Simulate queries and collect metrics
	for (int i = 0; i < 5; i++) {
		auto start = std::chrono::high_resolution_clock::now();
		auto result = con.Query("SELECT COUNT(*) FROM test_table WHERE id % 2 = 0");
		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double> duration = end - start;

		SelfTuningConfig::PerformanceMetrics metrics;
		metrics.avg_query_latency = duration.count();
		metrics.system_throughput = 1.0 / duration.count();
		metrics.active_queries = 1;
		metrics.completed_queries = i + 1;

		config.UpdateMetrics(metrics);
		REQUIRE(CHECK_COLUMN(result, 0, {5000}));
	}

	// Perform auto-tuning
	config.AutoTune();

	// Verify parameters are reasonable
	auto params = config.GetParameters();
	REQUIRE(params.default_morsel_size > 0);
	REQUIRE(params.default_morsel_size <= 200);
	REQUIRE(params.ema_alpha > 0.0);
	REQUIRE(params.ema_alpha <= 1.0);
}
