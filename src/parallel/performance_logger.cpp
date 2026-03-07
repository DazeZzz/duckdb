#include "duckdb/parallel/performance_logger.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

PerformanceLogger::PerformanceLogger() {
}

PerformanceLogger::~PerformanceLogger() {
}

void PerformanceLogger::SetEnabled(bool enabled_p) {
	enabled = enabled_p;
}

std::chrono::time_point<std::chrono::high_resolution_clock> PerformanceLogger::Now() {
	return std::chrono::high_resolution_clock::now();
}

double PerformanceLogger::DurationMs(std::chrono::time_point<std::chrono::high_resolution_clock> start,
                                     std::chrono::time_point<std::chrono::high_resolution_clock> end) {
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	return static_cast<double>(duration.count()) / 1000.0;
}

void PerformanceLogger::LogTaskStart(idx_t task_id, TaskPhase phase, idx_t priority, double pass_value) {
	if (!enabled.load()) {
		return;
	}

	lock_guard<mutex> guard(task_metrics_lock);
	TaskMetrics metrics;
	metrics.task_id = task_id;
	metrics.phase = phase;
	metrics.priority = priority;
	metrics.pass_value = pass_value;
	metrics.start_time = Now();
	metrics.wait_time_ms = 0.0; // Will be calculated later if needed
	pending_tasks[task_id] = metrics;
}

void PerformanceLogger::LogTaskEnd(idx_t task_id, double execution_time_ms) {
	if (!enabled.load()) {
		return;
	}

	lock_guard<mutex> guard(task_metrics_lock);
	auto it = pending_tasks.find(task_id);
	if (it != pending_tasks.end()) {
		it->second.end_time = Now();
		it->second.execution_time_ms = execution_time_ms;
		task_metrics.push_back(it->second);
		pending_tasks.erase(it);
	}
}

void PerformanceLogger::LogQueryStart(idx_t query_id) {
	if (!enabled.load()) {
		return;
	}

	lock_guard<mutex> guard(query_metrics_lock);
	PerformanceQueryMetrics metrics;
	metrics.query_id = query_id;
	metrics.start_time = Now();
	pending_queries[query_id] = metrics;
}

void PerformanceLogger::LogQueryEnd(idx_t query_id, double response_time_ms, idx_t total_tasks,
                                    idx_t elastic_tasks, idx_t inelastic_tasks) {
	if (!enabled.load()) {
		return;
	}

	lock_guard<mutex> guard(query_metrics_lock);
	auto it = pending_queries.find(query_id);
	if (it != pending_queries.end()) {
		it->second.end_time = Now();
		it->second.response_time_ms = response_time_ms;
		it->second.total_tasks = total_tasks;
		it->second.elastic_tasks = elastic_tasks;
		it->second.inelastic_tasks = inelastic_tasks;
		query_metrics.push_back(it->second);
		pending_queries.erase(it);
	}
}

void PerformanceLogger::LogSchedulingDecision(idx_t resource_group_id, TaskPhase phase, double pass_value,
                                              idx_t total_groups, idx_t elastic_groups, idx_t inelastic_groups) {
	if (!enabled.load()) {
		return;
	}

	lock_guard<mutex> guard(scheduling_decisions_lock);
	SchedulingDecision decision;
	decision.timestamp = Now();
	decision.selected_resource_group_id = resource_group_id;
	decision.selected_phase = phase;
	decision.selected_pass_value = pass_value;
	decision.total_resource_groups = total_groups;
	decision.elastic_groups = elastic_groups;
	decision.inelastic_groups = inelastic_groups;
	scheduling_decisions.push_back(decision);
}

void PerformanceLogger::ExportTaskMetricsCSV(const string &filename) {
	lock_guard<mutex> guard(task_metrics_lock);

	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	// CSV header
	file << "task_id,phase,priority,execution_time_ms,pass_value\n";

	// Data rows
	for (const auto &metrics : task_metrics) {
		file << metrics.task_id << ","
		     << (metrics.phase == TaskPhase::ELASTIC ? "ELASTIC" : "INELASTIC") << ","
		     << metrics.priority << ","
		     << metrics.execution_time_ms << ","
		     << metrics.pass_value << "\n";
	}

	file.close();
}

void PerformanceLogger::ExportQueryMetricsCSV(const string &filename) {
	lock_guard<mutex> guard(query_metrics_lock);

	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	// CSV header
	file << "query_id,response_time_ms,total_tasks,elastic_tasks,inelastic_tasks\n";

	// Data rows
	for (const auto &metrics : query_metrics) {
		file << metrics.query_id << ","
		     << metrics.response_time_ms << ","
		     << metrics.total_tasks << ","
		     << metrics.elastic_tasks << ","
		     << metrics.inelastic_tasks << "\n";
	}

	file.close();
}

void PerformanceLogger::ExportSchedulingDecisionsCSV(const string &filename) {
	lock_guard<mutex> guard(scheduling_decisions_lock);

	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	// CSV header
	file << "timestamp_us,resource_group_id,phase,pass_value,total_groups,elastic_groups,inelastic_groups\n";

	// Data rows
	auto base_time = scheduling_decisions.empty() ? Now() : scheduling_decisions[0].timestamp;
	for (const auto &decision : scheduling_decisions) {
		auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
			decision.timestamp - base_time).count();
		file << timestamp_us << ","
		     << decision.selected_resource_group_id << ","
		     << (decision.selected_phase == TaskPhase::ELASTIC ? "ELASTIC" : "INELASTIC") << ","
		     << decision.selected_pass_value << ","
		     << decision.total_resource_groups << ","
		     << decision.elastic_groups << ","
		     << decision.inelastic_groups << "\n";
	}

	file.close();
}

void PerformanceLogger::ExportAllMetricsJSON(const string &filename) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		return;
	}

	file << "{\n";
	file << "  \"task_metrics\": [\n";

	{
		lock_guard<mutex> guard(task_metrics_lock);
		for (size_t i = 0; i < task_metrics.size(); i++) {
			const auto &m = task_metrics[i];
			file << "    {\n";
			file << "      \"task_id\": " << m.task_id << ",\n";
			file << "      \"phase\": \"" << (m.phase == TaskPhase::ELASTIC ? "ELASTIC" : "INELASTIC") << "\",\n";
			file << "      \"priority\": " << m.priority << ",\n";
			file << "      \"execution_time_ms\": " << m.execution_time_ms << ",\n";
			file << "      \"pass_value\": " << m.pass_value << "\n";
			file << "    }" << (i < task_metrics.size() - 1 ? "," : "") << "\n";
		}
	}

	file << "  ],\n";
	file << "  \"query_metrics\": [\n";

	{
		lock_guard<mutex> guard(query_metrics_lock);
		for (size_t i = 0; i < query_metrics.size(); i++) {
			const auto &m = query_metrics[i];
			file << "    {\n";
			file << "      \"query_id\": " << m.query_id << ",\n";
			file << "      \"response_time_ms\": " << m.response_time_ms << ",\n";
			file << "      \"total_tasks\": " << m.total_tasks << ",\n";
			file << "      \"elastic_tasks\": " << m.elastic_tasks << ",\n";
			file << "      \"inelastic_tasks\": " << m.inelastic_tasks << "\n";
			file << "    }" << (i < query_metrics.size() - 1 ? "," : "") << "\n";
		}
	}

	file << "  ]\n";
	file << "}\n";

	file.close();
}

idx_t PerformanceLogger::GetTotalTasks() const {
	lock_guard<mutex> guard(task_metrics_lock);
	return task_metrics.size();
}

idx_t PerformanceLogger::GetTotalQueries() const {
	lock_guard<mutex> guard(query_metrics_lock);
	return query_metrics.size();
}

idx_t PerformanceLogger::GetTotalSchedulingDecisions() const {
	lock_guard<mutex> guard(scheduling_decisions_lock);
	return scheduling_decisions.size();
}

void PerformanceLogger::Clear() {
	{
		lock_guard<mutex> guard(task_metrics_lock);
		task_metrics.clear();
		pending_tasks.clear();
	}
	{
		lock_guard<mutex> guard(query_metrics_lock);
		query_metrics.clear();
		pending_queries.clear();
	}
	{
		lock_guard<mutex> guard(scheduling_decisions_lock);
		scheduling_decisions.clear();
	}
}

} // namespace duckdb
