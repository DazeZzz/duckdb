//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/phase_classifier.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/enums/physical_operator_type.hpp"
#include "duckdb/parallel/task_phase.hpp"

namespace duckdb {

class PhysicalOperator;
class Pipeline;

//! PhaseClassifier determines whether operators/pipelines are ELASTIC or INELASTIC
//! Based on Berg et al. "The case for phase-aware scheduling of parallelizable jobs"
class PhaseClassifier {
public:
	//! Classify a single physical operator
	static TaskPhase ClassifyOperator(PhysicalOperatorType type);

	//! Classify a pipeline based on its operators
	//! Returns INELASTIC if any operator in the pipeline is INELASTIC
	//! This is conservative: if there's any serial work, treat the whole pipeline as INELASTIC
	static TaskPhase ClassifyPipeline(const Pipeline &pipeline);

private:
	//! Check if an operator type is INELASTIC (serial or limited parallelism)
	static bool IsInelastic(PhysicalOperatorType type);
};

} // namespace duckdb
