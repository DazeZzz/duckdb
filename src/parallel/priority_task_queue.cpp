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
	// Select the resource group with the minimum pass value
	// This implements the stride scheduling algorithm
	shared_ptr<ResourceGroup> selected_group;
	double min_pass = std::numeric_limits<double>::max();

	for (auto &group : resource_groups) {
		if (!group->HasTasks()) {
			continue;
		}
		double pass = group->GetPass();
		if (pass < min_pass) {
			min_pass = pass;
			selected_group = group;
		}
	}

	return selected_group;
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
