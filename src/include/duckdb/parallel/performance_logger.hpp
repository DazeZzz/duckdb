//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/performance_logger.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/parallel/task_phase.hpp"
#include <chrono>
#include <fstream>

namespace duckdb {

//! Performance metrics for a single task execution
struct TaskMetrics {
	idx_t task_id;
	TaskPhase phase;
	idx_t priority;
	double execution_time_ms;
	double wait_time_ms;
	double pass_value;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
};

//! Performance metrics for a query execution
struct PerformanceQueryMetrics {
	idx_t query_id;
	double response_time_ms;
	idx_t total_tasks;
	idx_t elastic_tasks;
	idx_t inelastic_tasks;
	double elastic_time_ms;
	double inelastic_time_ms;
	double avg_task_execution_time_ms;
	double scheduling_overhead_ms;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
};

//! Scheduling decision record
struct SchedulingDecision {
	std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
	idx_t selected_resource_group_id;
	TaskPhase selected_phase;
	double selected_pass_value;
	idx_t total_resource_groups;
	idx_t elastic_groups;
	idx_t inelastic_groups;
};

//! PerformanceLogger collects and exports performance metrics for experimental analysis
class PerformanceLogger {
public:
	PerformanceLogger();
	~PerformanceLogger();

	//! Enable or disable logging
	void SetEnabled(bool enabled);
	bool IsEnabled() const {
		return enabled.load();
	}

	//! Task-level logging
	void LogTaskStart(idx_t task_id, TaskPhase phase, idx_t priority, double pass_value);
	void LogTaskEnd(idx_t task_id, double execution_time_ms);

	//! Query-level logging
	void LogQueryStart(idx_t query_id);
	void LogQueryEnd(idx_t query_id, double response_time_ms, idx_t total_tasks, idx_t elastic_tasks,
	                 idx_t inelastic_tasks);

	//! Scheduling decision logging
	void LogSchedulingDecision(idx_t resource_group_id, TaskPhase phase, double pass_value,
	                           idx_t total_groups, idx_t elastic_groups, idx_t inelastic_groups);

	//! Export collected data
	void ExportTaskMetricsCSV(const string &filename);
	void ExportQueryMetricsCSV(const string &filename);
	void ExportSchedulingDecisionsCSV(const string &filename);
	void ExportAllMetricsJSON(const string &filename);

	//! Get statistics
	idx_t GetTotalTasks() const;
	idx_t GetTotalQueries() const;
	idx_t GetTotalSchedulingDecisions() const;

	//! Clear all collected data
	void Clear();

private:
	//! Whether logging is enabled
	atomic<bool> enabled{false};

	//! Task metrics
	mutable mutex task_metrics_lock;
	vector<TaskMetrics> task_metrics;
	unordered_map<idx_t, TaskMetrics> pending_tasks; // task_id -> metrics

	//! Query metrics
	mutable mutex query_metrics_lock;
	vector<PerformanceQueryMetrics> query_metrics;
	unordered_map<idx_t, PerformanceQueryMetrics> pending_queries; // query_id -> metrics

	//! Scheduling decisions
	mutable mutex scheduling_decisions_lock;
	vector<SchedulingDecision> scheduling_decisions;

	//! Helper: Get current timestamp
	static std::chrono::time_point<std::chrono::high_resolution_clock> Now();

	//! Helper: Calculate duration in milliseconds
	static double DurationMs(std::chrono::time_point<std::chrono::high_resolution_clock> start,
	                         std::chrono::time_point<std::chrono::high_resolution_clock> end);
};

} // namespace duckdb
