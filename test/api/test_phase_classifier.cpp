#include "catch.hpp"
#include "duckdb/parallel/phase_classifier.hpp"
#include "duckdb/common/enums/physical_operator_type.hpp"

using namespace duckdb;

TEST_CASE("Test phase classification for operators", "[parallel][phase_aware]") {
	// Test ELASTIC operators (perfectly parallelizable)
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::TABLE_SCAN) == TaskPhase::ELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::FILTER) == TaskPhase::ELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::PROJECTION) == TaskPhase::ELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::NESTED_LOOP_JOIN) == TaskPhase::ELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::STREAMING_LIMIT) == TaskPhase::ELASTIC);

	// Test INELASTIC operators (serial or limited parallelism)
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::ORDER_BY) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::TOP_N) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::HASH_GROUP_BY) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::PERFECT_HASH_GROUP_BY) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::WINDOW) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::HASH_JOIN) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::RECURSIVE_CTE) == TaskPhase::INELASTIC);
	REQUIRE(PhaseClassifier::ClassifyOperator(PhysicalOperatorType::RESERVOIR_SAMPLE) == TaskPhase::INELASTIC);
}
