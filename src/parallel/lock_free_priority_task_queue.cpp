#include "duckdb/parallel/lock_free_priority_task_queue.hpp"

#include <algorithm>
#include <thread>

namespace duckdb {

LockFreePriorityTaskQueue::LockFreePriorityTaskQueue() {
	// Initialize with an empty vector
	resource_groups_ptr = new vector<shared_ptr<ResourceGroup>>();
}

LockFreePriorityTaskQueue::~LockFreePriorityTaskQueue() {
	auto *groups = resource_groups_ptr.load();
	delete groups;
}

void LockFreePriorityTaskQueue::AcquireModificationLock() {
	// Simple spin lock using atomic CAS
	bool expected = false;
	while (!modification_lock.compare_exchange_weak(expected, true)) {
		expected = false;
		// Yield to avoid busy waiting
		std::this_thread::yield();
	}
}

void LockFreePriorityTaskQueue::ReleaseModificationLock() {
	modification_lock = false;
}

void LockFreePriorityTaskQueue::RegisterResourceGroup(shared_ptr<ResourceGroup> resource_group) {
	AcquireModificationLock();

	// Create a new vector with the new resource group
	auto *old_groups = resource_groups_ptr.load();
	auto *new_groups = new vector<shared_ptr<ResourceGroup>>(*old_groups);
	new_groups->push_back(std::move(resource_group));

	// Atomically swap the pointer
	resource_groups_ptr = new_groups;

	ReleaseModificationLock();

	// Delete old vector (safe because we hold the modification lock)
	delete old_groups;
}

void LockFreePriorityTaskQueue::UnregisterResourceGroup(shared_ptr<ResourceGroup> resource_group) {
	AcquireModificationLock();

	// Create a new vector without the resource group
	auto *old_groups = resource_groups_ptr.load();
	auto *new_groups = new vector<shared_ptr<ResourceGroup>>();

	for (auto &group : *old_groups) {
		if (group != resource_group) {
			new_groups->push_back(group);
		}
	}

	// Atomically swap the pointer
	resource_groups_ptr = new_groups;

	ReleaseModificationLock();

	// Delete old vector
	delete old_groups;
}

void LockFreePriorityTaskQueue::EnqueueTask(shared_ptr<ResourceGroup> resource_group, shared_ptr<Task> task) {
	// Enqueue the task to the resource group's internal queue
	// ResourceGroup's EnqueueTask is already thread-safe with its own lock
	resource_group->EnqueueTask(std::move(task));
}

shared_ptr<ResourceGroup> LockFreePriorityTaskQueue::SelectResourceGroup() {
	// Lock-free selection using atomic load
	// We read the current pointer atomically
	auto *groups = resource_groups_ptr.load();

	// Implement Inelastic-First (IF) strategy from Berg et al.
	// Priority 1: Select inelastic tasks first (non-parallelizable work)
	// Priority 2: Among same phase, select by minimum pass value (stride scheduling)

	shared_ptr<ResourceGroup> selected_inelastic;
	shared_ptr<ResourceGroup> selected_elastic;
	double min_pass_inelastic = std::numeric_limits<double>::max();
	double min_pass_elastic = std::numeric_limits<double>::max();

	// First pass: separate inelastic and elastic groups, find minimum pass in each
	for (auto &group : *groups) {
		if (!group->HasTasks()) {
			continue;
		}

		double pass = group->GetPass();
		TaskPhase phase = group->GetPhase();

		if (phase == TaskPhase::INELASTIC) {
			if (pass < min_pass_inelastic) {
				min_pass_inelastic = pass;
				selected_inelastic = group;
			}
		} else { // ELASTIC
			if (pass < min_pass_elastic) {
				min_pass_elastic = pass;
				selected_elastic = group;
			}
		}
	}

	// Inelastic-First: prioritize inelastic tasks
	if (selected_inelastic) {
		return selected_inelastic;
	}

	// If no inelastic tasks, return elastic task
	return selected_elastic;
}

bool LockFreePriorityTaskQueue::DequeueTask(shared_ptr<Task> &task, shared_ptr<ResourceGroup> &resource_group) {
	// Lock-free dequeue: we don't need a lock for reading the resource groups
	// because we use atomic pointer load

	// Select the resource group with minimum pass value
	resource_group = SelectResourceGroup();
	if (!resource_group) {
		return false;
	}

	// Dequeue a task from the selected resource group
	// ResourceGroup's DequeueTask is already thread-safe with its own lock
	return resource_group->DequeueTask(task);
}

idx_t LockFreePriorityTaskQueue::GetTaskCount() const {
	auto *groups = resource_groups_ptr.load();
	idx_t total = 0;
	for (auto &group : *groups) {
		total += group->GetTaskCount();
	}
	return total;
}

// Adaptive priorities implementation (Phase 3)

void LockFreePriorityTaskQueue::SetAdaptivePriorities(bool enabled) {
	auto *groups = resource_groups_ptr.load();
	for (auto &group : *groups) {
		group->SetAdaptivePriorities(enabled);
	}
}

void LockFreePriorityTaskQueue::UpdateAdaptivePriorities() {
	auto *groups = resource_groups_ptr.load();

	if (groups->empty()) {
		return;
	}

	// Calculate global average throughput
	double total_throughput = 0.0;
	idx_t active_groups = 0;

	for (auto &group : *groups) {
		double throughput = group->GetThroughput();
		if (throughput > 0.0) {
			total_throughput += throughput;
			active_groups++;
		}
	}

	if (active_groups == 0) {
		// No throughput data yet
		return;
	}

	double avg_throughput = total_throughput / static_cast<double>(active_groups);
	average_throughput = avg_throughput;

	// Adapt priorities for all resource groups
	for (auto &group : *groups) {
		group->AdaptPriority(avg_throughput);
	}
}

} // namespace duckdb
