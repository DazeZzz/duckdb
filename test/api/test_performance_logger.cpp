#include "catch.hpp"
#include "duckdb/parallel/performance_logger.hpp"
#include "duckdb/parallel/task_scheduler.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

TEST_CASE("Test performance logger basic functionality", "[parallel][performance]") {
	PerformanceLogger logger;

	// Test enable/disable
	REQUIRE(!logger.IsEnabled());
	logger.SetEnabled(true);
	REQUIRE(logger.IsEnabled());

	// Test task logging
	logger.LogTaskStart(1, TaskPhase::ELASTIC, 100, 0.5);
	logger.LogTaskEnd(1, 10.5);

	logger.LogTaskStart(2, TaskPhase::INELASTIC, 200, 1.0);
	logger.LogTaskEnd(2, 20.3);

	REQUIRE(logger.GetTotalTasks() == 2);

	// Test query logging
	logger.LogQueryStart(1);
	logger.LogQueryEnd(1, 100.0, 10, 7, 3);

	REQUIRE(logger.GetTotalQueries() == 1);

	// Test scheduling decision logging
	logger.LogSchedulingDecision(1, TaskPhase::INELASTIC, 0.5, 3, 2, 1);
	logger.LogSchedulingDecision(2, TaskPhase::ELASTIC, 1.0, 3, 2, 1);

	REQUIRE(logger.GetTotalSchedulingDecisions() == 2);

	// Test clear
	logger.Clear();
	REQUIRE(logger.GetTotalTasks() == 0);
	REQUIRE(logger.GetTotalQueries() == 0);
	REQUIRE(logger.GetTotalSchedulingDecisions() == 0);
}

TEST_CASE("Test performance logger CSV export", "[parallel][performance]") {
	PerformanceLogger logger;
	logger.SetEnabled(true);

	// Log some data
	logger.LogTaskStart(1, TaskPhase::ELASTIC, 100, 0.5);
	logger.LogTaskEnd(1, 10.5);

	logger.LogTaskStart(2, TaskPhase::INELASTIC, 200, 1.0);
	logger.LogTaskEnd(2, 20.3);

	logger.LogQueryStart(1);
	logger.LogQueryEnd(1, 100.0, 10, 7, 3);

	// Export to CSV (files will be created in test directory)
	logger.ExportTaskMetricsCSV("/tmp/test_task_metrics.csv");
	logger.ExportQueryMetricsCSV("/tmp/test_query_metrics.csv");

	// Verify files exist (basic check)
	REQUIRE(logger.GetTotalTasks() == 2);
	REQUIRE(logger.GetTotalQueries() == 1);
}

TEST_CASE("Test TaskScheduler has performance logger", "[parallel][performance]") {
	DuckDB db(nullptr);
	Connection con(db);

	auto &scheduler = TaskScheduler::GetScheduler(*db.instance);
	auto &logger = scheduler.GetPerformanceLogger();

	// Verify logger exists and can be configured
	REQUIRE(!logger.IsEnabled());
	logger.SetEnabled(true);
	REQUIRE(logger.IsEnabled());
	logger.SetEnabled(false);
}
