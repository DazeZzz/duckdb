#include "catch.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/priority_task_queue.hpp"
#include "duckdb/parallel/task.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

namespace {

// Simple test task
class TestTask : public Task {
public:
	explicit TestTask(int id) : task_id(id), executed(false) {
	}

	TaskExecutionResult Execute(TaskExecutionMode mode) override {
		executed = true;
		return TaskExecutionResult::TASK_FINISHED;
	}

	int task_id;
	bool executed;
};

} // namespace

TEST_CASE("Test ResourceGroup basic functionality", "[parallel][resource_group]") {
	// Test creation with valid priority
	auto rg = make_shared_ptr<ResourceGroup>(10000);
	REQUIRE(rg->GetPriority() == 10000);
	REQUIRE(rg->GetPass() == 0.0);
	REQUIRE(rg->GetTaskCount() == 0);
	REQUIRE(!rg->HasTasks());

	// Test priority change
	rg->SetPriority(5000);
	REQUIRE(rg->GetPriority() == 5000);

	// Test task enqueue/dequeue
	auto task1 = make_shared_ptr<TestTask>(1);
	auto task2 = make_shared_ptr<TestTask>(2);

	rg->EnqueueTask(task1);
	REQUIRE(rg->GetTaskCount() == 1);
	REQUIRE(rg->HasTasks());

	rg->EnqueueTask(task2);
	REQUIRE(rg->GetTaskCount() == 2);

	shared_ptr<Task> dequeued_task;
	REQUIRE(rg->DequeueTask(dequeued_task));
	REQUIRE(dequeued_task != nullptr);
	REQUIRE(rg->GetTaskCount() == 1);

	REQUIRE(rg->DequeueTask(dequeued_task));
	REQUIRE(rg->GetTaskCount() == 0);
	REQUIRE(!rg->HasTasks());

	// Test dequeue from empty queue
	REQUIRE(!rg->DequeueTask(dequeued_task));
}

TEST_CASE("Test ResourceGroup pass value updates", "[parallel][resource_group]") {
	auto rg = make_shared_ptr<ResourceGroup>(10000);
	REQUIRE(rg->GetPass() == 0.0);

	// Simulate executing a task that took 1.0 normalized time units
	rg->UpdatePass(1.0);

	// stride = 1/10000 = 0.0001
	// pass should be 0.0 + 1.0 * 0.0001 = 0.0001
	REQUIRE(rg->GetPass() == Approx(0.0001));

	// Test with different priority
	auto rg2 = make_shared_ptr<ResourceGroup>(5000);
	rg2->UpdatePass(1.0);
	// stride = 1/5000 = 0.0002
	// pass should be 0.0 + 1.0 * 0.0002 = 0.0002
	REQUIRE(rg2->GetPass() == Approx(0.0002));

	// rg has higher priority (smaller stride), so after executing the same amount of work,
	// rg should have a lower pass value and be selected first
	REQUIRE(rg->GetPass() < rg2->GetPass());

	// Execute another task on rg
	rg->UpdatePass(1.0);
	REQUIRE(rg->GetPass() == Approx(0.0002));
}

TEST_CASE("Test PriorityTaskQueue stride scheduling", "[parallel][priority_task_queue]") {
	PriorityTaskQueue queue;

	// Create two resource groups with different priorities
	auto rg_high = make_shared_ptr<ResourceGroup>(10000); // Higher priority (smaller stride)
	auto rg_low = make_shared_ptr<ResourceGroup>(5000);   // Lower priority (larger stride)

	queue.RegisterResourceGroup(rg_high);
	queue.RegisterResourceGroup(rg_low);

	// Add tasks to both groups
	auto task_high_1 = make_shared_ptr<TestTask>(1);
	auto task_high_2 = make_shared_ptr<TestTask>(2);
	auto task_low_1 = make_shared_ptr<TestTask>(3);
	auto task_low_2 = make_shared_ptr<TestTask>(4);

	queue.EnqueueTask(rg_high, task_high_1);
	queue.EnqueueTask(rg_high, task_high_2);
	queue.EnqueueTask(rg_low, task_low_1);
	queue.EnqueueTask(rg_low, task_low_2);

	REQUIRE(queue.GetTaskCount() == 4);

	// Dequeue tasks and verify stride scheduling
	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	// First task should be from high priority group (both have pass=0, but we'll select high first)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(task != nullptr);
	REQUIRE(selected_rg == rg_high);

	// Simulate execution and update pass
	selected_rg->UpdatePass(1.0); // pass = 0.0001

	// Second task should be from low priority group (pass=0 < 0.0001)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg_low);
	selected_rg->UpdatePass(1.0); // pass = 0.0002

	// Third task should be from high priority group (pass=0.0001 < 0.0002)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg_high);
	selected_rg->UpdatePass(1.0); // pass = 0.0002

	// Fourth task: both have pass=0.0002, either could be selected
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(task != nullptr);

	// No more tasks
	REQUIRE(queue.GetTaskCount() == 0);
	REQUIRE(!queue.DequeueTask(task, selected_rg));
}

TEST_CASE("Test PriorityTaskQueue with multiple resource groups", "[parallel][priority_task_queue]") {
	PriorityTaskQueue queue;

	// Create three resource groups with different priorities
	auto rg1 = make_shared_ptr<ResourceGroup>(10000); // Highest priority
	auto rg2 = make_shared_ptr<ResourceGroup>(5000);  // Medium priority
	auto rg3 = make_shared_ptr<ResourceGroup>(2500);  // Lowest priority

	queue.RegisterResourceGroup(rg1);
	queue.RegisterResourceGroup(rg2);
	queue.RegisterResourceGroup(rg3);

	// Add one task to each group
	queue.EnqueueTask(rg1, make_shared_ptr<TestTask>(1));
	queue.EnqueueTask(rg2, make_shared_ptr<TestTask>(2));
	queue.EnqueueTask(rg3, make_shared_ptr<TestTask>(3));

	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	// Simulate multiple rounds of execution
	for (int round = 0; round < 10; round++) {
		// Add more tasks
		queue.EnqueueTask(rg1, make_shared_ptr<TestTask>(10 + round));
		queue.EnqueueTask(rg2, make_shared_ptr<TestTask>(20 + round));
		queue.EnqueueTask(rg3, make_shared_ptr<TestTask>(30 + round));

		// Execute 3 tasks
		for (int i = 0; i < 3; i++) {
			REQUIRE(queue.DequeueTask(task, selected_rg));
			selected_rg->UpdatePass(1.0);
		}
	}

	// Verify that high priority group got more CPU time
	// rg1 (priority 10000, stride 0.0001) should have lowest pass value
	// rg3 (priority 2500, stride 0.0004) should have highest pass value
	REQUIRE(rg1->GetPass() < rg2->GetPass());
	REQUIRE(rg2->GetPass() < rg3->GetPass());

	// The ratio of pass values should approximately match the inverse priority ratio
	// rg1:rg2 = 10000:5000 = 2:1, so pass_rg2 should be ~2x pass_rg1
	double ratio = rg2->GetPass() / rg1->GetPass();
	REQUIRE(ratio > 1.8);
	REQUIRE(ratio < 2.2);
}

TEST_CASE("Test ResourceGroup statistics", "[parallel][resource_group]") {
	auto rg = make_shared_ptr<ResourceGroup>(10000);

	REQUIRE(rg->GetTotalExecutedTasks() == 0);
	REQUIRE(rg->GetTotalExecutionTime() == 0.0);

	// Simulate executing tasks
	rg->UpdatePass(1.0);
	REQUIRE(rg->GetTotalExecutedTasks() == 1);
	REQUIRE(rg->GetTotalExecutionTime() == Approx(1.0));

	rg->UpdatePass(2.0);
	REQUIRE(rg->GetTotalExecutedTasks() == 2);
	REQUIRE(rg->GetTotalExecutionTime() == Approx(3.0));

	rg->UpdatePass(0.5);
	REQUIRE(rg->GetTotalExecutedTasks() == 3);
	REQUIRE(rg->GetTotalExecutionTime() == Approx(3.5));
}

TEST_CASE("Test phase-aware scheduling (Inelastic-First)", "[parallel][priority_task_queue][phase_aware]") {
	PriorityTaskQueue queue;

	// Create two resource groups with same priority but different phases
	auto rg_elastic = make_shared_ptr<ResourceGroup>(10000);
	auto rg_inelastic = make_shared_ptr<ResourceGroup>(10000);

	// Set phases
	rg_elastic->SetPhase(TaskPhase::ELASTIC);
	rg_inelastic->SetPhase(TaskPhase::INELASTIC);

	queue.RegisterResourceGroup(rg_elastic);
	queue.RegisterResourceGroup(rg_inelastic);

	// Add tasks to both groups
	queue.EnqueueTask(rg_elastic, make_shared_ptr<TestTask>(1));
	queue.EnqueueTask(rg_inelastic, make_shared_ptr<TestTask>(2));

	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	// First task should be from inelastic group (Inelastic-First strategy)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg_inelastic);
	REQUIRE(dynamic_cast<TestTask *>(task.get())->task_id == 2);

	// Update pass value
	selected_rg->UpdatePass(1.0);

	// Second task should be from elastic group (no more inelastic tasks)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg_elastic);
	REQUIRE(dynamic_cast<TestTask *>(task.get())->task_id == 1);
}

TEST_CASE("Test phase-aware scheduling with multiple inelastic tasks", "[parallel][priority_task_queue][phase_aware]") {
	PriorityTaskQueue queue;

	// Create three resource groups
	auto rg1 = make_shared_ptr<ResourceGroup>(10000); // High priority, elastic
	auto rg2 = make_shared_ptr<ResourceGroup>(5000);  // Low priority, inelastic
	auto rg3 = make_shared_ptr<ResourceGroup>(2500);  // Very low priority, inelastic

	rg1->SetPhase(TaskPhase::ELASTIC);
	rg2->SetPhase(TaskPhase::INELASTIC);
	rg3->SetPhase(TaskPhase::INELASTIC);

	queue.RegisterResourceGroup(rg1);
	queue.RegisterResourceGroup(rg2);
	queue.RegisterResourceGroup(rg3);

	// Add tasks
	queue.EnqueueTask(rg1, make_shared_ptr<TestTask>(1));
	queue.EnqueueTask(rg2, make_shared_ptr<TestTask>(2));
	queue.EnqueueTask(rg3, make_shared_ptr<TestTask>(3));

	shared_ptr<Task> task;
	shared_ptr<ResourceGroup> selected_rg;

	// First task should be from rg2 (inelastic, higher priority than rg3)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg2);
	selected_rg->UpdatePass(1.0);

	// Second task should be from rg3 (inelastic, even though rg1 has higher priority)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg3);
	selected_rg->UpdatePass(1.0);

	// Third task should be from rg1 (elastic, no more inelastic tasks)
	REQUIRE(queue.DequeueTask(task, selected_rg));
	REQUIRE(selected_rg == rg1);
}
