//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/execution_state.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types.hpp"

namespace duckdb {

//! Execution state for adaptive morsel execution
//! Based on Wagner et al. SIGMOD 2021 - Self-Tuning Query Scheduling
enum class ExecutionState : uint8_t {
	//! Startup phase: Initial execution with smaller morsel size to estimate throughput
	STARTUP = 0,
	//! Default phase: Normal execution with standard morsel size
	DEFAULT = 1,
	//! Shutdown phase: Final phase with photo finish mechanism (smaller morsel size)
	SHUTDOWN = 2
};

//! Configuration constants for adaptive morsel execution
namespace AdaptiveMorselConfig {
	//! Morsel size for startup phase (number of chunks)
	constexpr idx_t STARTUP_MORSEL_SIZE = 10;
	//! Morsel size for default phase (number of chunks)
	constexpr idx_t DEFAULT_MORSEL_SIZE = 50;
	//! Morsel size for shutdown phase (number of chunks)
	constexpr idx_t SHUTDOWN_MORSEL_SIZE = 10;

	//! Threshold for transitioning from Startup to Default (number of chunks processed)
	constexpr idx_t STARTUP_THRESHOLD = 100;
	//! Threshold for transitioning from Default to Shutdown (progress percentage)
	constexpr double SHUTDOWN_THRESHOLD = 0.9;

	//! Alpha for exponential moving average (EMA) of throughput
	constexpr double EMA_ALPHA = 0.3;
}

} // namespace duckdb
