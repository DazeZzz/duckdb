//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/priority_task_queue.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/task.hpp"

namespace duckdb {

//! Simple priority-based task queue for Phase 1
//! Uses a lock-based approach with stride scheduling algorithm
//! This will be replaced with a lock-free implementation in Phase 4
class PriorityTaskQueue {
public:
	PriorityTaskQueue();
	~PriorityTaskQueue();

	//! Register a resource group (query) with the scheduler
	void RegisterResourceGroup(shared_ptr<ResourceGroup> resource_group);

	//! Unregister a resource group when the query completes
	void UnregisterResourceGroup(shared_ptr<ResourceGroup> resource_group);

	//! Enqueue a task to a resource group
	void EnqueueTask(shared_ptr<ResourceGroup> resource_group, shared_ptr<Task> task);

	//! Dequeue the highest priority task
	//! Returns true if a task was dequeued, false if no tasks available
	bool DequeueTask(shared_ptr<Task> &task, shared_ptr<ResourceGroup> &resource_group);

	//! Get the total number of tasks in the queue
	idx_t GetTaskCount() const;

	//! Check if there are any tasks available
	bool HasTasks() const {
		return GetTaskCount() > 0;
	}

	//! Adaptive priorities (Phase 3)
	//! Enable or disable adaptive priorities for all resource groups
	void SetAdaptivePriorities(bool enabled);

	//! Update global average throughput and adapt priorities
	void UpdateAdaptivePriorities();

	//! Get the global average throughput
	double GetAverageThroughput() const {
		return average_throughput.load();
	}

private:
	//! Select the resource group with the minimum pass value
	shared_ptr<ResourceGroup> SelectResourceGroup();

	//! Lock for accessing the resource groups
	mutable mutex queue_lock;

	//! List of active resource groups (queries)
	vector<shared_ptr<ResourceGroup>> resource_groups;

	//! Adaptive priorities (Phase 3)
	//! Global average throughput across all resource groups
	atomic<double> average_throughput{0.0};
};

} // namespace duckdb
