#include "duckdb/parallel/priority_task_queue.hpp"

#include <algorithm>

namespace duckdb {

PriorityTaskQueue::PriorityTaskQueue() {
}

PriorityTaskQueue::~PriorityTaskQueue() {
}

void PriorityTaskQueue::RegisterResourceGroup(shared_ptr<ResourceGroup> resource_group) {
	lock_guard<mutex> guard(queue_lock);
	resource_groups.push_back(std::move(resource_group));
}

void PriorityTaskQueue::UnregisterResourceGroup(shared_ptr<ResourceGroup> resource_group) {
	lock_guard<mutex> guard(queue_lock);
	auto it = std::find(resource_groups.begin(), resource_groups.end(), resource_group);
	if (it != resource_groups.end()) {
		resource_groups.erase(it);
	}
}

void PriorityTaskQueue::EnqueueTask(shared_ptr<ResourceGroup> resource_group, shared_ptr<Task> task) {
	// Enqueue the task to the resource group's internal queue
	resource_group->EnqueueTask(std::move(task));
}

shared_ptr<ResourceGroup> PriorityTaskQueue::SelectResourceGroup() {
	// Implement Inelastic-First (IF) strategy from Berg et al.
	// Priority 1: Select inelastic tasks first (non-parallelizable work)
	// Priority 2: Among same phase, select by minimum pass value (stride scheduling)

	shared_ptr<ResourceGroup> selected_inelastic;
	shared_ptr<ResourceGroup> selected_elastic;
	double min_pass_inelastic = std::numeric_limits<double>::max();
	double min_pass_elastic = std::numeric_limits<double>::max();

	// First pass: separate inelastic and elastic groups, find minimum pass in each
	for (auto &group : resource_groups) {
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

bool PriorityTaskQueue::DequeueTask(shared_ptr<Task> &task, shared_ptr<ResourceGroup> &resource_group) {
	lock_guard<mutex> guard(queue_lock);

	// Select the resource group with minimum pass value
	resource_group = SelectResourceGroup();
	if (!resource_group) {
		return false;
	}

	// Dequeue a task from the selected resource group
	return resource_group->DequeueTask(task);
}

idx_t PriorityTaskQueue::GetTaskCount() const {
	lock_guard<mutex> guard(queue_lock);
	idx_t total = 0;
	for (auto &group : resource_groups) {
		total += group->GetTaskCount();
	}
	return total;
}

} // namespace duckdb
