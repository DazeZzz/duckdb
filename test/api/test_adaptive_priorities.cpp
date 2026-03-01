#include "catch.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/priority_task_queue.hpp"
#include "test_helpers.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test throughput tracking in ResourceGroup", "[adaptive_priorities]") {
	auto group = make_shared_ptr<ResourceGroup>(10);

	// Initially throughput should be 0
	REQUIRE(group->GetThroughput() == 0.0);

	// Update throughput with first measurement
	group->UpdateThroughput(100, 1.0); // 100 tasks in 1 second = 100 tasks/sec
	REQUIRE(group->GetThroughput() == 100.0);

	// Update with second measurement (should use EMA)
	group->UpdateThroughput(80, 1.0); // 80 tasks in 1 second = 80 tasks/sec
	// EMA = 0.3 * 80 + 0.7 * 100 = 24 + 70 = 94
	REQUIRE(group->GetThroughput() == Approx(94.0));

	// Update with third measurement
	group->UpdateThroughput(120, 1.0); // 120 tasks in 1 second = 120 tasks/sec
	// EMA = 0.3 * 120 + 0.7 * 94 = 36 + 65.8 = 101.8
	REQUIRE(group->GetThroughput() == Approx(101.8));
}

TEST_CASE("Test adaptive priority adjustment", "[adaptive_priorities]") {
	auto group = make_shared_ptr<ResourceGroup>(10);
	group->SetAdaptivePriorities(true);

	// Set initial throughput
	group->UpdateThroughput(100, 1.0); // 100 tasks/sec

	// Test 1: Throughput equal to average -> priority stays the same
	group->AdaptPriority(100.0); // avg_throughput = 100
	REQUIRE(group->GetPriority() == 10);

	// Test 2: Throughput higher than average -> priority increases
	group->UpdateThroughput(200, 1.0); // Higher throughput
	group->AdaptPriority(100.0);
	// new_priority = 10 * (throughput / avg) = 10 * (higher / 100) > 10
	REQUIRE(group->GetPriority() > 10);

	// Test 3: Throughput lower than average -> priority decreases
	auto group2 = make_shared_ptr<ResourceGroup>(10);
	group2->SetAdaptivePriorities(true);
	group2->UpdateThroughput(50, 1.0); // Lower throughput
	group2->AdaptPriority(100.0);
	// new_priority = 10 * (50 / 100) = 5
	REQUIRE(group2->GetPriority() == 5);
}

TEST_CASE("Test priority bounds", "[adaptive_priorities]") {
	auto group = make_shared_ptr<ResourceGroup>(10);
	group->SetAdaptivePriorities(true);

	// Test upper bound: priority should not exceed base_priority * 4
	group->UpdateThroughput(1000, 1.0); // Very high throughput
	group->AdaptPriority(10.0); // Very low average
	// Should be clamped to 10 * 4 = 40
	REQUIRE(group->GetPriority() <= 40);

	// Test lower bound: priority should not go below base_priority * 0.25
	auto group2 = make_shared_ptr<ResourceGroup>(10);
	group2->SetAdaptivePriorities(true);
	group2->UpdateThroughput(1, 1.0); // Very low throughput
	group2->AdaptPriority(1000.0); // Very high average
	// Should be clamped to 10 * 0.25 = 2.5 -> 2 (integer)
	REQUIRE(group2->GetPriority() >= 2);
}

TEST_CASE("Test adaptive priorities disabled", "[adaptive_priorities]") {
	auto group = make_shared_ptr<ResourceGroup>(10);
	// Adaptive priorities disabled by default
	REQUIRE(!group->IsAdaptivePrioritiesEnabled());

	group->UpdateThroughput(200, 1.0);
	group->AdaptPriority(100.0);

	// Priority should not change when adaptive priorities are disabled
	REQUIRE(group->GetPriority() == 10);
}

TEST_CASE("Test global average throughput calculation", "[adaptive_priorities]") {
	PriorityTaskQueue queue;

	auto group1 = make_shared_ptr<ResourceGroup>(10);
	auto group2 = make_shared_ptr<ResourceGroup>(20);
	auto group3 = make_shared_ptr<ResourceGroup>(30);

	queue.RegisterResourceGroup(group1);
	queue.RegisterResourceGroup(group2);
	queue.RegisterResourceGroup(group3);

	// Set throughputs
	group1->UpdateThroughput(100, 1.0); // 100 tasks/sec
	group2->UpdateThroughput(200, 1.0); // 200 tasks/sec
	group3->UpdateThroughput(150, 1.0); // 150 tasks/sec

	// Update adaptive priorities
	queue.UpdateAdaptivePriorities();

	// Average should be (100 + 200 + 150) / 3 = 150
	REQUIRE(queue.GetAverageThroughput() == Approx(150.0));
}

TEST_CASE("Test adaptive priorities with PriorityTaskQueue", "[adaptive_priorities]") {
	PriorityTaskQueue queue;

	auto group1 = make_shared_ptr<ResourceGroup>(10);
	auto group2 = make_shared_ptr<ResourceGroup>(10);

	queue.RegisterResourceGroup(group1);
	queue.RegisterResourceGroup(group2);

	// Enable adaptive priorities
	queue.SetAdaptivePriorities(true);

	// Set different throughputs
	group1->UpdateThroughput(200, 1.0); // High throughput
	group2->UpdateThroughput(50, 1.0);  // Low throughput

	// Update adaptive priorities
	queue.UpdateAdaptivePriorities();

	// Average = (200 + 50) / 2 = 125
	// group1: priority = 10 * (200 / 125) = 16
	// group2: priority = 10 * (50 / 125) = 4
	REQUIRE(group1->GetPriority() == 16);
	REQUIRE(group2->GetPriority() == 4);

	// Verify stride is updated correctly
	// stride = 1 / priority
	REQUIRE(group1->GetPass() == 0.0); // Pass starts at 0
	group1->UpdatePass(1.0);
	// pass = 0 + 1.0 * (1/16) = 0.0625
	REQUIRE(group1->GetPass() == Approx(0.0625));

	group2->UpdatePass(1.0);
	// pass = 0 + 1.0 * (1/4) = 0.25
	REQUIRE(group2->GetPass() == Approx(0.25));
}

TEST_CASE("Test adaptive priorities with realistic workload", "[adaptive_priorities]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create test tables
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE fast_table AS SELECT range AS id FROM range(1000)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE slow_table AS SELECT range AS id FROM range(10000)"));

	// Execute queries with different characteristics
	auto result1 = con.Query("SELECT COUNT(*) FROM fast_table");
	REQUIRE(CHECK_COLUMN(result1, 0, {1000}));

	auto result2 = con.Query("SELECT COUNT(*) FROM slow_table WHERE id % 2 = 0");
	REQUIRE(CHECK_COLUMN(result2, 0, {5000}));

	// Test with join (more complex workload)
	auto result3 = con.Query(
	    "SELECT COUNT(*) FROM fast_table f JOIN slow_table s ON f.id = s.id WHERE f.id < 500");
	REQUIRE(CHECK_COLUMN(result3, 0, {500}));
}

TEST_CASE("Test starvation prevention", "[adaptive_priorities]") {
	PriorityTaskQueue queue;

	auto high_throughput_group = make_shared_ptr<ResourceGroup>(10);
	auto low_throughput_group = make_shared_ptr<ResourceGroup>(10);

	queue.RegisterResourceGroup(high_throughput_group);
	queue.RegisterResourceGroup(low_throughput_group);

	queue.SetAdaptivePriorities(true);

	// Simulate extreme throughput difference
	high_throughput_group->UpdateThroughput(10000, 1.0); // Very high
	low_throughput_group->UpdateThroughput(10, 1.0);     // Very low

	queue.UpdateAdaptivePriorities();

	// Even with extreme difference, low throughput group should have at least
	// base_priority * 0.25 = 10 * 0.25 = 2.5 -> 2
	REQUIRE(low_throughput_group->GetPriority() >= 2);

	// High throughput group should be capped at base_priority * 4 = 40
	REQUIRE(high_throughput_group->GetPriority() <= 40);
}
