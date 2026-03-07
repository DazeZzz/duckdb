#include "duckdb/parallel/phase_classifier.hpp"
#include "duckdb/parallel/pipeline.hpp"
#include "duckdb/execution/physical_operator.hpp"

namespace duckdb {

bool PhaseClassifier::IsInelastic(PhysicalOperatorType type) {
	switch (type) {
	// Sorting operations - inherently serial or limited parallelism
	case PhysicalOperatorType::ORDER_BY:
	case PhysicalOperatorType::TOP_N:
		return true;

	// Aggregation operations - finalize phase is serial
	case PhysicalOperatorType::HASH_GROUP_BY:
	case PhysicalOperatorType::PERFECT_HASH_GROUP_BY:
	case PhysicalOperatorType::UNGROUPED_AGGREGATE:
	case PhysicalOperatorType::PARTITIONED_AGGREGATE:
		return true;

	// Window functions - partitioning and ordering are serial
	case PhysicalOperatorType::WINDOW:
	case PhysicalOperatorType::STREAMING_WINDOW:
		return true;

	// Hash join build phase - building hash table is serial
	// Note: We can't distinguish build vs probe here, so we conservatively
	// mark all hash joins as INELASTIC. In practice, the sink (build) phase
	// will be INELASTIC while the source (probe) phase could be ELASTIC.
	case PhysicalOperatorType::HASH_JOIN:
		return true;

	// Recursive operations - inherently serial
	case PhysicalOperatorType::RECURSIVE_CTE:
	case PhysicalOperatorType::RECURSIVE_KEY_CTE:
		return true;

	// Pivot operations - require global coordination
	case PhysicalOperatorType::PIVOT:
		return true;

	// Sample operations that require reservoir sampling (serial)
	case PhysicalOperatorType::RESERVOIR_SAMPLE:
		return true;

	// All other operators are considered ELASTIC (perfectly parallelizable)
	default:
		return false;
	}
}

TaskPhase PhaseClassifier::ClassifyOperator(PhysicalOperatorType type) {
	return IsInelastic(type) ? TaskPhase::INELASTIC : TaskPhase::ELASTIC;
}

TaskPhase PhaseClassifier::ClassifyPipeline(const Pipeline &pipeline) {
	// Get all operators in the pipeline (source, intermediate, sink)
	auto operators = pipeline.GetOperators();

	// Conservative approach: if ANY operator is INELASTIC, the whole pipeline is INELASTIC
	// This ensures we prioritize pipelines with serial bottlenecks
	for (const auto &op_ref : operators) {
		const auto &op = op_ref.get();
		if (IsInelastic(op.type)) {
			return TaskPhase::INELASTIC;
		}
	}

	// All operators are ELASTIC - pipeline is perfectly parallelizable
	return TaskPhase::ELASTIC;
}

} // namespace duckdb
