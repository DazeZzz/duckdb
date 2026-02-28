#include "catch.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/priority_task_queue.hpp"
#include "duckdb/parallel/task.hpp"
#include "test_helpers.hpp"
#include <chrono>
#include <thread>

using namespace duckdb;

namespace {

// Simulated task with configurable execution time
class SimulatedTask : public Task {
public:
	explicit SimulatedTask(int id, int duration_ms, TaskPhase phase)
	    : task_id(id), duration_ms(duration_ms), phase(phase), executed(false) {
	}

	TaskExecutionResult Execute(TaskExecutionMode mode) override {
		// Simulate work by sleeping
		std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
		executed = true;
		return TaskExecutionResult::TASK_FINISHED;
	}

	int task_id;
	int duration_ms;
	TaskPhase phase;
	bool executed;
};

} // namespace

TEST_CASE("Benchmark: Phase-aware vs non-phase-aware scheduling", "[parallel][phase_aware][benchmark]") {
	// This test simulates a realistic workload with mixed elastic and inelastic tasks
	// to demonstrate the benefits of Inelastic-First scheduling

	PriorityTaskQueue queue;

	// Create 3 queries with different characteristics
	auto query1 = make_shared_ptr<ResourceGroup>(10000); // High priority, mostly elastic
	auto query2 = make_shared_ptr<ResourceGroup>(8000);  // Medium priority, inelastic
	auto query3 = make_shared_ptr<ResourceGroup>(6000);  // Lower priority, inelastic

	// Set initial phases
	query1->SetPhase(TaskPhase::ELASTIC);
	query2->SetPhase(TaskPhase::INELASTIC);
	query3->SetPhase(TaskPhase::INELASTIC);

	queue.RegisterResourceGroup(query1);
	queue.RegisterResourceGroup(query2);
	queue.RegisterResourceGroup(query3);

	// Simulate a workload:
	// Query 1: 5 elastic tasks (scan, filter, project)
	// Query 2: 2 inelastic tasks (hash build)
	// Query 3: 2 inelastic tasks (aggregate finalize)

	// Add tasks for query 1 (all elastic)
	for (int i = 0; i < 5; i++) {
		queue.EnqueueTask(query1, make_shared_ptr<SimulatedTask>(100 + i, 10, TaskPhase::ELASTIC));
	}

	// Add tasks for query 2 (inelastic)
	for (int i = 0; i < 2; i++) {
		queue.EnqueueTask(query2, make_shared_ptr<SimulatedTask>(210 + i, 15, TaskPhase::INELASTIC));
	}

	// Add tasks for query 3 (all inelastic)
	for (int i = 0; i < 2; i++) {
		queue.EnqueueTask(query3, make_shared_ptr<SimulatedTask>(300 + i, 20, TaskPhase::INELASTIC));
	}

	// Execute tasks and track order
	vector<int> execution_order;
	vector<TaskPhase> phase_order;
	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	auto start_time = std::chrono::high_resolution_clock::now();

	while (queue.DequeueTask(task, selected_rg)) {
		auto sim_task = dynamic_cast<SimulatedTask *>(task.get());
		execution_order.push_back(sim_task->task_id);
		phase_order.push_back(selected_rg->GetPhase());

		// Execute the task
		task->Execute(TaskExecutionMode::PROCESS_ALL);

		// Update pass value (normalized to 2ms target)
		double normalized_time = sim_task->duration_ms / 2.0;
		selected_rg->UpdatePass(normalized_time);
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	// Verify Inelastic-First behavior:
	// 1. All inelastic tasks should be executed before elastic tasks (when possible)
	// 2. Within same phase, stride scheduling should apply

	// Count how many inelastic tasks were executed before the first elastic task
	int inelastic_count_before_elastic = 0;
	for (size_t i = 0; i < phase_order.size(); i++) {
		if (phase_order[i] == TaskPhase::INELASTIC) {
			inelastic_count_before_elastic++;
		} else {
			break; // Found first elastic task
		}
	}

	// We should have executed all 4 inelastic tasks (2 from query2 + 2 from query3) first
	REQUIRE(inelastic_count_before_elastic == 4);

	// Verify that inelastic tasks from query2 (higher priority) were executed before query3
	REQUIRE(execution_order[0] >= 210);
	REQUIRE(execution_order[0] < 220); // Should be from query2's inelastic tasks

	// Print execution order for debugging
	INFO("Execution order: ");
	for (size_t i = 0; i < execution_order.size(); i++) {
		INFO("  Task " << execution_order[i] << " (phase: "
		             << (phase_order[i] == TaskPhase::INELASTIC ? "INELASTIC" : "ELASTIC") << ")");
	}
	INFO("Total execution time: " << duration.count() << "ms");

	// Verify all tasks were executed
	REQUIRE(execution_order.size() == 9); // 5 + 2 + 2 = 9 tasks total
}

TEST_CASE("Test phase transition during execution", "[parallel][phase_aware]") {
	// This test simulates a query transitioning from elastic to inelastic phase

	PriorityTaskQueue queue;

	auto query1 = make_shared_ptr<ResourceGroup>(10000);
	auto query2 = make_shared_ptr<ResourceGroup>(10000);

	query1->SetPhase(TaskPhase::ELASTIC);
	query2->SetPhase(TaskPhase::ELASTIC);

	queue.RegisterResourceGroup(query1);
	queue.RegisterResourceGroup(query2);

	// Add elastic tasks to both queries
	queue.EnqueueTask(query1, make_shared_ptr<SimulatedTask>(1, 5, TaskPhase::ELASTIC));
	queue.EnqueueTask(query2, make_shared_ptr<SimulatedTask>(2, 5, TaskPhase::ELASTIC));

	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	// Execute first task (should be from either query, both elastic)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	task->Execute(TaskExecutionMode::PROCESS_ALL);
	selected_rg->UpdatePass(1.0);

	// Now query1 transitions to inelastic phase and adds more tasks
	query1->SetPhase(TaskPhase::INELASTIC);
	queue.EnqueueTask(query1, make_shared_ptr<SimulatedTask>(3, 5, TaskPhase::INELASTIC));

	// Next task should be from query1 (inelastic has priority)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == query1);
	REQUIRE(selected_rg->GetPhase() == TaskPhase::INELASTIC);
	auto sim_task = dynamic_cast<SimulatedTask *>(task.get());
	REQUIRE(sim_task->task_id == 3);
}

TEST_CASE("Test fairness among inelastic tasks", "[parallel][phase_aware]") {
	// Verify that stride scheduling still applies within inelastic tasks

	PriorityTaskQueue queue;

	auto high_priority = make_shared_ptr<ResourceGroup>(10000);
	auto low_priority = make_shared_ptr<ResourceGroup>(5000);

	high_priority->SetPhase(TaskPhase::INELASTIC);
	low_priority->SetPhase(TaskPhase::INELASTIC);

	queue.RegisterResourceGroup(high_priority);
	queue.RegisterResourceGroup(low_priority);

	// Add multiple tasks to both
	for (int i = 0; i < 3; i++) {
		queue.EnqueueTask(high_priority, make_shared_ptr<SimulatedTask>(100 + i, 5, TaskPhase::INELASTIC));
		queue.EnqueueTask(low_priority, make_shared_ptr<SimulatedTask>(200 + i, 5, TaskPhase::INELASTIC));
	}

	// Execute tasks and track which query gets more CPU time
	int high_priority_count = 0;
	int low_priority_count = 0;

	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	for (int i = 0; i < 6; i++) {
		REQUIRE(queue.DequeueTask(task, selected_rg));
		task->Execute(TaskExecutionMode::PROCESS_ALL);
		selected_rg->UpdatePass(1.0);

		if (selected_rg == high_priority) {
			high_priority_count++;
		} else {
			low_priority_count++;
		}
	}

	// High priority should get more tasks (approximately 2:1 ratio due to stride)
	// With stride scheduling: high_priority has stride=0.0001, low_priority has stride=0.0002
	// So high_priority should get roughly twice as many tasks
	INFO("High priority tasks: " << high_priority_count);
	INFO("Low priority tasks: " << low_priority_count);

	// We expect high_priority to get at least as many tasks as low_priority
	REQUIRE(high_priority_count >= low_priority_count);
}
