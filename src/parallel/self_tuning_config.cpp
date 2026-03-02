#include "duckdb/parallel/self_tuning_config.hpp"

#include <algorithm>
#include <chrono>

namespace duckdb {

SelfTuningConfig::SelfTuningConfig() {
	// Initialize with default parameters
	ResetToDefaults();
}

SelfTuningConfig::~SelfTuningConfig() {
}

void SelfTuningConfig::ResetToDefaults() {
	lock_guard<mutex> guard(params_lock);
	current_params = TunableParameters();
}

SelfTuningConfig::PerformanceMetrics SelfTuningConfig::GetMetrics() const {
	PerformanceMetrics metrics;
	metrics.avg_query_latency = avg_query_latency.load();
	metrics.system_throughput = system_throughput.load();
	metrics.cpu_utilization = cpu_utilization.load();
	metrics.active_queries = active_queries.load();
	metrics.completed_queries = completed_queries.load();
	metrics.avg_task_execution_time = avg_task_execution_time.load();
	return metrics;
}

void SelfTuningConfig::UpdateMetrics(const PerformanceMetrics &metrics) {
	// Update atomic metrics
	avg_query_latency = metrics.avg_query_latency;
	system_throughput = metrics.system_throughput;
	cpu_utilization = metrics.cpu_utilization;
	active_queries = metrics.active_queries;
	completed_queries = metrics.completed_queries;
	avg_task_execution_time = metrics.avg_task_execution_time;

	// Add to history for trend analysis
	lock_guard<mutex> guard(history_lock);
	metrics_history.push_back(metrics);
	if (metrics_history.size() > HISTORY_SIZE) {
		metrics_history.erase(metrics_history.begin());
	}
}

SelfTuningConfig::TunableParameters SelfTuningConfig::GetParameters() const {
	lock_guard<mutex> guard(params_lock);
	return current_params;
}

void SelfTuningConfig::SetParameters(const TunableParameters &params) {
	lock_guard<mutex> guard(params_lock);
	current_params = params;
}

void SelfTuningConfig::SetAutoTuning(bool enabled) {
	auto_tuning_enabled = enabled;
}

void SelfTuningConfig::AutoTune() {
	if (!auto_tuning_enabled.load()) {
		return;
	}

	// Check if enough time has passed since last tuning
	auto now = std::chrono::high_resolution_clock::now();
	auto now_seconds = std::chrono::duration<double>(now.time_since_epoch()).count();
	double last_time = last_tuning_time.load();

	TunableParameters params = GetParameters();
	if (now_seconds - last_time < params.tuning_interval) {
		return;
	}

	last_tuning_time = now_seconds;

	// Perform tuning adjustments
	AnalyzeTrends();
	AdjustMorselSize();
	AdjustPriorityBounds();
	AdjustEmaAlpha();
}

void SelfTuningConfig::AnalyzeTrends() {
	lock_guard<mutex> guard(history_lock);

	if (metrics_history.size() < 3) {
		// Not enough data for trend analysis
		return;
	}

	// Analyze recent trends (last 3 measurements)
	// This is a simple implementation - could be enhanced with more sophisticated analysis
}

void SelfTuningConfig::AdjustMorselSize() {
	auto metrics = GetMetrics();
	lock_guard<mutex> guard(params_lock);

	// Adjust morsel size based on workload characteristics
	// High CPU utilization + low latency -> increase morsel size (reduce overhead)
	// Low CPU utilization + high latency -> decrease morsel size (improve responsiveness)

	if (metrics.cpu_utilization > 0.9 && metrics.avg_query_latency < 1.0) {
		// System is busy but responsive - increase morsel size to reduce overhead
		current_params.default_morsel_size = std::min<idx_t>(100, current_params.default_morsel_size + 10);
	} else if (metrics.cpu_utilization < 0.5 && metrics.avg_query_latency > 2.0) {
		// System is underutilized and slow - decrease morsel size for better responsiveness
		current_params.default_morsel_size = std::max<idx_t>(20, current_params.default_morsel_size - 10);
	}

	// Adjust startup morsel size proportionally
	current_params.startup_morsel_size = current_params.default_morsel_size / 5;
	current_params.shutdown_morsel_size = current_params.default_morsel_size / 5;
}

void SelfTuningConfig::AdjustPriorityBounds() {
	auto metrics = GetMetrics();
	lock_guard<mutex> guard(params_lock);

	// Adjust priority bounds based on fairness and performance
	// Many active queries -> tighten bounds for better fairness
	// Few active queries -> widen bounds for better differentiation

	if (metrics.active_queries > 10) {
		// Many queries - tighten bounds for fairness
		current_params.min_priority_factor = std::min(0.5, current_params.min_priority_factor + 0.05);
		current_params.max_priority_factor = std::max(2.0, current_params.max_priority_factor - 0.2);
	} else if (metrics.active_queries < 3) {
		// Few queries - widen bounds for differentiation
		current_params.min_priority_factor = std::max(0.1, current_params.min_priority_factor - 0.05);
		current_params.max_priority_factor = std::min(8.0, current_params.max_priority_factor + 0.2);
	}
}

void SelfTuningConfig::AdjustEmaAlpha() {
	lock_guard<mutex> history_guard(history_lock);

	if (metrics_history.size() < 5) {
		return;
	}

	// Calculate variance in throughput to determine stability
	double sum = 0.0;
	double sum_sq = 0.0;
	for (const auto &m : metrics_history) {
		sum += m.system_throughput;
		sum_sq += m.system_throughput * m.system_throughput;
	}

	double mean = sum / metrics_history.size();
	double variance = (sum_sq / metrics_history.size()) - (mean * mean);
	double std_dev = std::sqrt(variance);

	lock_guard<mutex> params_guard(params_lock);

	// High variance -> increase alpha (react faster to changes)
	// Low variance -> decrease alpha (smooth out noise)
	if (std_dev > mean * 0.3) {
		// High variance - increase alpha
		current_params.ema_alpha = std::min(0.5, current_params.ema_alpha + 0.05);
	} else if (std_dev < mean * 0.1) {
		// Low variance - decrease alpha
		current_params.ema_alpha = std::max(0.1, current_params.ema_alpha - 0.05);
	}
}

} // namespace duckdb
