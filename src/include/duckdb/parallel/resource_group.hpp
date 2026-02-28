//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/resource_group.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/common.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/queue.hpp"
#include "duckdb/parallel/task.hpp"

namespace duckdb {

//! ResourceGroup represents a query with its priority and scheduling state
//! This is the core abstraction for query-level scheduling
class ResourceGroup {
public:
	explicit ResourceGroup(idx_t initial_priority);
	~ResourceGroup();

	//! Get the current priority of this resource group
	idx_t GetPriority() const {
		return priority.load();
	}

	//! Set the priority of this resource group
	void SetPriority(idx_t new_priority);

	//! Get the current pass value (virtual time)
	double GetPass() const {
		return pass.load();
	}

	//! Update the pass value after executing a task
	//! execution_time: normalized execution time (relative to target duration)
	void UpdatePass(double execution_time);

	//! Enqueue a task to this resource group
	void EnqueueTask(shared_ptr<Task> task);

	//! Try to dequeue a task from this resource group
	//! Returns true if a task was dequeued, false if no tasks available
	bool DequeueTask(shared_ptr<Task> &task);

	//! Get the number of tasks in the queue
	idx_t GetTaskCount() const;

	//! Check if this resource group has any tasks
	bool HasTasks() const {
		return GetTaskCount() > 0;
	}

	//! Get statistics
	idx_t GetTotalExecutedTasks() const {
		return total_executed_tasks.load();
	}

	double GetTotalExecutionTime() const {
		return total_execution_time.load();
	}

private:
	//! Priority of this resource group (higher = more important)
	atomic<idx_t> priority;

	//! Stride value (1 / priority)
	atomic<double> stride;

	//! Pass value (virtual time) - used for scheduling decisions
	atomic<double> pass;

	//! Queue of tasks for this resource group
	mutable mutex task_queue_lock;
	std::queue<shared_ptr<Task>> task_queue;

	//! Statistics
	atomic<idx_t> total_executed_tasks;
	atomic<double> total_execution_time;
};

} // namespace duckdb
