#include "duckdb/parallel/resource_group.hpp"

#include "duckdb/common/exception.hpp"

namespace duckdb {

ResourceGroup::ResourceGroup(idx_t initial_priority)
    : priority(initial_priority), stride(1.0 / static_cast<double>(initial_priority)), pass(0.0),
      current_phase(TaskPhase::ELASTIC), total_executed_tasks(0), total_execution_time(0.0) {
	if (initial_priority == 0) {
		throw InternalException("ResourceGroup priority cannot be zero");
	}
}

ResourceGroup::~ResourceGroup() {
}

void ResourceGroup::SetPriority(idx_t new_priority) {
	if (new_priority == 0) {
		throw InternalException("ResourceGroup priority cannot be zero");
	}
	priority = new_priority;
	stride = 1.0 / static_cast<double>(new_priority);
}

void ResourceGroup::UpdatePass(double execution_time) {
	// Update pass value: pass += execution_time * stride
	// This is the core of the stride scheduling algorithm
	double current_stride = stride.load();
	double current_pass = pass.load();
	double new_pass = current_pass + execution_time * current_stride;

	// Use compare-and-swap to update atomically
	while (!pass.compare_exchange_weak(current_pass, new_pass)) {
		// If CAS failed, recalculate with the new current_pass
		new_pass = current_pass + execution_time * current_stride;
	}

	// Update statistics
	total_executed_tasks++;
	double current_time = total_execution_time.load();
	while (!total_execution_time.compare_exchange_weak(current_time, current_time + execution_time)) {
		// Retry if CAS failed
	}
}

void ResourceGroup::EnqueueTask(shared_ptr<Task> task) {
	lock_guard<mutex> guard(task_queue_lock);
	task_queue.push(std::move(task));
}

bool ResourceGroup::DequeueTask(shared_ptr<Task> &task) {
	lock_guard<mutex> guard(task_queue_lock);
	if (task_queue.empty()) {
		return false;
	}
	task = std::move(task_queue.front());
	task_queue.pop();
	return true;
}

idx_t ResourceGroup::GetTaskCount() const {
	lock_guard<mutex> guard(task_queue_lock);
	return task_queue.size();
}

} // namespace duckdb
