//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/self_tuning_config.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

//! Self-tuning configuration for adaptive scheduler
//! Based on Wagner et al. SIGMOD 2021 - Self-Tuning Query Scheduling
//! Automatically adjusts parameters based on workload characteristics and performance metrics
class SelfTuningConfig {
public:
	SelfTuningConfig();
	~SelfTuningConfig();

	//! Performance metrics
	struct PerformanceMetrics {
		double avg_query_latency = 0.0;      // Average query response time (seconds)
		double system_throughput = 0.0;       // System-wide throughput (queries/sec)
		double cpu_utilization = 0.0;         // CPU utilization (0.0 - 1.0)
		idx_t active_queries = 0;             // Number of active queries
		idx_t completed_queries = 0;          // Total completed queries
		double avg_task_execution_time = 0.0; // Average task execution time
	};

	//! Tunable parameters
	struct TunableParameters {
		// Adaptive morsel execution parameters
		idx_t startup_morsel_size = 10;
		idx_t default_morsel_size = 50;
		idx_t shutdown_morsel_size = 10;
		idx_t startup_threshold = 100;
		double shutdown_threshold = 0.9;
		double ema_alpha = 0.3;

		// Adaptive priority parameters
		double min_priority_factor = 0.25;
		double max_priority_factor = 4.0;

		// Self-tuning parameters
		double tuning_interval = 1.0; // Seconds between tuning adjustments
		bool auto_tuning_enabled = true;
	};

	//! Get current performance metrics
	PerformanceMetrics GetMetrics() const;

	//! Update performance metrics
	void UpdateMetrics(const PerformanceMetrics &metrics);

	//! Get current tunable parameters
	TunableParameters GetParameters() const;

	//! Set tunable parameters
	void SetParameters(const TunableParameters &params);

	//! Perform automatic tuning based on current metrics
	void AutoTune();

	//! Enable or disable auto-tuning
	void SetAutoTuning(bool enabled);

	//! Check if auto-tuning is enabled
	bool IsAutoTuningEnabled() const {
		return auto_tuning_enabled.load();
	}

	//! Reset to default parameters
	void ResetToDefaults();

private:
	//! Current performance metrics
	atomic<double> avg_query_latency{0.0};
	atomic<double> system_throughput{0.0};
	atomic<double> cpu_utilization{0.0};
	atomic<idx_t> active_queries{0};
	atomic<idx_t> completed_queries{0};
	atomic<double> avg_task_execution_time{0.0};

	//! Tunable parameters (protected by mutex for atomic updates)
	mutable mutex params_lock;
	TunableParameters current_params;

	//! Auto-tuning state
	atomic<bool> auto_tuning_enabled{true};
	atomic<double> last_tuning_time{0.0};

	//! Historical metrics for trend analysis
	static constexpr idx_t HISTORY_SIZE = 10;
	mutable mutex history_lock;
	vector<PerformanceMetrics> metrics_history;

	//! Helper: Analyze performance trends
	void AnalyzeTrends();

	//! Helper: Adjust morsel size based on workload
	void AdjustMorselSize();

	//! Helper: Adjust priority bounds based on fairness
	void AdjustPriorityBounds();

	//! Helper: Adjust EMA alpha based on stability
	void AdjustEmaAlpha();
};

} // namespace duckdb
