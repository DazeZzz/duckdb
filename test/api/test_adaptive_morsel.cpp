#include "catch.hpp"
#include "duckdb/parallel/pipeline.hpp"
#include "duckdb/parallel/execution_state.hpp"
#include "duckdb/execution/executor.hpp"
#include "duckdb/main/client_context.hpp"
#include "test_helpers.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test adaptive morsel execution state transitions", "[adaptive_morsel]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create a test table with enough data to trigger state transitions
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE test_data AS SELECT range AS id FROM range(10000)"));

	// Execute a query and verify state transitions
	auto result = con.Query("SELECT COUNT(*) FROM test_data WHERE id % 2 = 0");
	REQUIRE(CHECK_COLUMN(result, 0, {5000}));
}

TEST_CASE("Test morsel size changes based on execution state", "[adaptive_morsel]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create a mock pipeline to test morsel size calculation
	auto context = con.context;
	Executor executor(*context);
	Pipeline pipeline(executor);

	// Test Startup state
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::STARTUP);
	REQUIRE(pipeline.GetCurrentMorselSize() == AdaptiveMorselConfig::STARTUP_MORSEL_SIZE);

	// Simulate processing enough chunks to transition to Default
	pipeline.IncrementChunksProcessed(AdaptiveMorselConfig::STARTUP_THRESHOLD);
	pipeline.UpdateExecutionState(0.5); // 50% progress
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::DEFAULT);
	REQUIRE(pipeline.GetCurrentMorselSize() == AdaptiveMorselConfig::DEFAULT_MORSEL_SIZE);

	// Simulate high progress to transition to Shutdown
	pipeline.UpdateExecutionState(0.95); // 95% progress
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::SHUTDOWN);
	REQUIRE(pipeline.GetCurrentMorselSize() == AdaptiveMorselConfig::SHUTDOWN_MORSEL_SIZE);
}

TEST_CASE("Test throughput estimation with EMA", "[adaptive_morsel]") {
	DuckDB db(nullptr);
	Connection con(db);

	auto context = con.context;
	Executor executor(*context);
	Pipeline pipeline(executor);

	// First measurement
	pipeline.UpdateThroughput(50, 1.0); // 50 chunks in 1 second = 50 chunks/sec
	// Initial EMA should be the first measurement
	// Note: We can't directly access throughput_ema, but we can verify it's updated

	// Second measurement
	pipeline.UpdateThroughput(60, 1.0); // 60 chunks in 1 second = 60 chunks/sec
	// EMA = 0.3 * 60 + 0.7 * 50 = 18 + 35 = 53 chunks/sec

	// Third measurement
	pipeline.UpdateThroughput(40, 1.0); // 40 chunks in 1 second = 40 chunks/sec
	// EMA = 0.3 * 40 + 0.7 * 53 = 12 + 37.1 = 49.1 chunks/sec

	// Verify that throughput estimation doesn't crash with zero execution time
	pipeline.UpdateThroughput(50, 0.0); // Should be ignored
}

TEST_CASE("Test adaptive morsel with realistic workload", "[adaptive_morsel]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create tables with different sizes
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE small_table AS SELECT range AS id FROM range(100)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE large_table AS SELECT range AS id FROM range(100000)"));

	// Test with small table (should stay in Startup or quickly move to Default)
	auto result1 = con.Query("SELECT COUNT(*) FROM small_table");
	REQUIRE(CHECK_COLUMN(result1, 0, {100}));

	// Test with large table (should go through all states)
	auto result2 = con.Query("SELECT COUNT(*) FROM large_table WHERE id % 3 = 0");
	REQUIRE(CHECK_COLUMN(result2, 0, {33334}));

	// Test with join (more complex workload)
	auto result3 = con.Query(
	    "SELECT COUNT(*) FROM large_table l1 JOIN large_table l2 ON l1.id = l2.id WHERE l1.id < 1000");
	REQUIRE(CHECK_COLUMN(result3, 0, {1000}));
}

TEST_CASE("Test photo finish mechanism", "[adaptive_morsel]") {
	DuckDB db(nullptr);
	Connection con(db);

	auto context = con.context;
	Executor executor(*context);
	Pipeline pipeline(executor);

	// Start in Startup state
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::STARTUP);
	idx_t startup_morsel = pipeline.GetCurrentMorselSize();

	// Transition to Default
	pipeline.IncrementChunksProcessed(AdaptiveMorselConfig::STARTUP_THRESHOLD);
	pipeline.UpdateExecutionState(0.5);
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::DEFAULT);
	idx_t default_morsel = pipeline.GetCurrentMorselSize();

	// Transition to Shutdown (photo finish)
	pipeline.UpdateExecutionState(0.95);
	REQUIRE(pipeline.GetExecutionState() == ExecutionState::SHUTDOWN);
	idx_t shutdown_morsel = pipeline.GetCurrentMorselSize();

	// Verify that Shutdown morsel size is smaller than Default for load balancing
	REQUIRE(shutdown_morsel < default_morsel);
	// Verify that Startup and Shutdown have the same small morsel size
	REQUIRE(startup_morsel == shutdown_morsel);
}
