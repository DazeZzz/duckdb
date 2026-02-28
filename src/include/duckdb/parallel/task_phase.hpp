//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/task_phase.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

//! TaskPhase represents the execution phase of a task
//! Based on Berg et al. "The case for phase-aware scheduling of parallelizable jobs"
enum class TaskPhase : uint8_t {
	//! Elastic phase: can be perfectly parallelized (e.g., scan, filter, project, hash probe)
	//! Running on k cores takes 1/k of the serial time
	ELASTIC = 0,

	//! Inelastic phase: serial or partially serial (e.g., hash build, aggregate finalize, sort)
	//! Can only use one core effectively, or has limited parallelism
	INELASTIC = 1
};

} // namespace duckdb
