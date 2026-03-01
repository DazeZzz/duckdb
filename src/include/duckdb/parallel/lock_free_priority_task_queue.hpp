//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/lock_free_priority_task_queue.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/common.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/parallel/resource_group.hpp"
#include "duckdb/parallel/task.hpp"

namespace duckdb {

//! Lock-free priority-based task queue for Phase 4
//! Uses atomic operations and CAS (Compare-And-Swap) for thread-safe scheduling
//! Implements the same stride scheduling + Inelastic-First strategy as PriorityTaskQueue
//! but without locks for better scalability
class LockFreePriorityTaskQueue {
public:
	LockFreePriorityTaskQueue();
	~LockFreePriorityTaskQueue();

	//! Register a resource group (query) with the scheduler
	void RegisterResourceGroup(shared_ptr<ResourceGroup> resource_group);

	//! Unregister a resource group when the query completes
	void UnregisterResourceGroup(shared_ptr<ResourceGroup> resource_group);

	//! Enqueue a task to a resource group
	void EnqueueTask(shared_ptr<ResourceGroup> resource_group, shared_ptr<Task> task);

	//! Dequeue the highest priority task (lock-free)
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
	//! Select the resource group with the minimum pass value (lock-free)
	shared_ptr<ResourceGroup> SelectResourceGroup();

	//! List of active resource groups (queries)
	//! We use atomic operations to access this list without locks
	atomic<vector<shared_ptr<ResourceGroup>>*> resource_groups_ptr;

	//! Spin lock for rare operations (register/unregister)
	//! We use a simple atomic flag for these infrequent operations
	atomic<bool> modification_lock{false};

	//! Adaptive priorities (Phase 3)
	//! Global average throughput across all resource groups
	atomic<double> average_throughput{0.0};

	//! Helper: Acquire modification lock using spin lock
	void AcquireModificationLock();

	//! Helper: Release modification lock
	void ReleaseModificationLock();
};

} // namespace duckdb
