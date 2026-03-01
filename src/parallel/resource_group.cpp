#include "duckdb/parallel/resource_group.hpp"

#include "duckdb/common/exception.hpp"

namespace duckdb {

ResourceGroup::ResourceGroup(idx_t initial_priority)
    : priority(initial_priority), stride(1.0 / static_cast<double>(initial_priority)), pass(0.0),
      current_phase(TaskPhase::ELASTIC), total_executed_tasks(0), total_execution_time(0.0),
      base_priority(initial_priority), throughput(0.0), adaptive_priorities_enabled(false) {
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

// Adaptive priorities implementation (Phase 3)

void ResourceGroup::UpdateThroughput(idx_t tasks_executed, double execution_time) {
	if (execution_time <= 0.0 || tasks_executed == 0) {
		return;
	}

	// Calculate current throughput (tasks per second)
	double current_throughput = static_cast<double>(tasks_executed) / execution_time;

	// Use exponential moving average (EMA) to smooth throughput estimation
	// EMA_alpha = 0.3 (same as adaptive morsel execution)
	constexpr double EMA_ALPHA = 0.3;
	double prev_throughput = throughput.load();

	if (prev_throughput == 0.0) {
		// First measurement
		throughput = current_throughput;
	} else {
		// EMA = alpha * current + (1 - alpha) * previous
		double new_throughput = EMA_ALPHA * current_throughput + (1.0 - EMA_ALPHA) * prev_throughput;
		throughput = new_throughput;
	}
}

void ResourceGroup::AdaptPriority(double avg_throughput) {
	if (!adaptive_priorities_enabled.load()) {
		return;
	}

	if (avg_throughput <= 0.0) {
		// No valid average throughput, keep current priority
		return;
	}

	double current_throughput = throughput.load();
	if (current_throughput <= 0.0) {
		// No throughput data yet, keep current priority
		return;
	}

	// Calculate throughput ratio
	double throughput_ratio = current_throughput / avg_throughput;

	// Adjust priority based on throughput ratio
	// Higher throughput -> higher priority (smaller stride)
	// Formula: new_priority = base_priority * throughput_ratio
	idx_t base_prio = base_priority.load();
	double new_priority_double = static_cast<double>(base_prio) * throughput_ratio;

	// Clamp priority to reasonable bounds [1, base_priority * 4]
	// This prevents extreme priority values
	constexpr double MIN_PRIORITY_FACTOR = 0.25; // At least 25% of base priority
	constexpr double MAX_PRIORITY_FACTOR = 4.0;  // At most 4x base priority

	double min_priority = static_cast<double>(base_prio) * MIN_PRIORITY_FACTOR;
	double max_priority = static_cast<double>(base_prio) * MAX_PRIORITY_FACTOR;

	new_priority_double = std::max(min_priority, std::min(max_priority, new_priority_double));

	// Convert to integer priority (at least 1)
	idx_t new_priority = std::max<idx_t>(1, static_cast<idx_t>(new_priority_double));

	// Update priority and stride
	priority = new_priority;
	stride = 1.0 / static_cast<double>(new_priority);
}

} // namespace duckdb
