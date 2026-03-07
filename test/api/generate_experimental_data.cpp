#include "duckdb.hpp"
#include "duckdb/parallel/task_scheduler.hpp"
#include "duckdb/parallel/performance_logger.hpp"
#include <iostream>
#include <chrono>
#include <fstream>

using namespace duckdb;
using namespace std;

// TPC-H Query 1: Pricing Summary Report
const char *TPCH_Q1 = R"(
SELECT
    l_returnflag,
    l_linestatus,
    sum(l_quantity) as sum_qty,
    sum(l_extendedprice) as sum_base_price,
    sum(l_extendedprice * (1 - l_discount)) as sum_disc_price,
    sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge,
    avg(l_quantity) as avg_qty,
    avg(l_extendedprice) as avg_price,
    avg(l_discount) as avg_disc,
    count(*) as count_order
FROM
    lineitem
WHERE
    l_shipdate <= CAST('1998-09-02' AS date)
GROUP BY
    l_returnflag,
    l_linestatus
ORDER BY
    l_returnflag,
    l_linestatus;
)";

// TPC-H Query 6: Forecasting Revenue Change
const char *TPCH_Q6 = R"(
SELECT
    sum(l_extendedprice * l_discount) as revenue
FROM
    lineitem
WHERE
    l_shipdate >= CAST('1994-01-01' AS date)
    AND l_shipdate < CAST('1995-01-01' AS date)
    AND l_discount BETWEEN 0.05 AND 0.07
    AND l_quantity < 24;
)";

struct ExperimentConfig {
	string name;
	bool enable_phase_aware;
	int num_runs;
	vector<string> queries;
};

struct ExperimentResult {
	string config_name;
	string query_name;
	int run_number;
	double response_time_ms;
	idx_t total_tasks;
	idx_t elastic_tasks;
	idx_t inelastic_tasks;
};

void SetupTPCHData(Connection &con, double scale_factor = 0.01) {
	cout << "Setting up TPC-H data (scale factor: " << scale_factor << ")..." << endl;

	// Install and load TPC-H extension
	con.Query("INSTALL tpch");
	con.Query("LOAD tpch");

	// Generate TPC-H data
	con.Query("CALL dbgen(sf=" + to_string(scale_factor) + ")");

	cout << "TPC-H data setup complete." << endl;
}

ExperimentResult RunQuery(Connection &con, const string &query_name, const string &query_sql,
                          const string &config_name, int run_number, PerformanceLogger &logger) {
	ExperimentResult result;
	result.config_name = config_name;
	result.query_name = query_name;
	result.run_number = run_number;

	// Clear previous metrics
	logger.Clear();
	logger.SetEnabled(true);

	// Record start time
	auto start = chrono::high_resolution_clock::now();

	// Execute query
	try {
		auto query_result = con.Query(query_sql);
		if (query_result->HasError()) {
			cerr << "Query error: " << query_result->GetError() << endl;
			result.response_time_ms = -1.0;
			return result;
		}
	} catch (const exception &e) {
		cerr << "Exception: " << e.what() << endl;
		result.response_time_ms = -1.0;
		return result;
	}

	// Record end time
	auto end = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
	result.response_time_ms = duration.count() / 1000.0;

	// Get metrics from logger
	result.total_tasks = logger.GetTotalTasks();
	// Note: We would need to parse task metrics to get elastic/inelastic counts
	// For now, use placeholder values
	result.elastic_tasks = 0;
	result.inelastic_tasks = 0;

	logger.SetEnabled(false);

	return result;
}

void RunExperiment(const ExperimentConfig &config, vector<ExperimentResult> &results) {
	cout << "\n=== Running Experiment: " << config.name << " ===" << endl;
	cout << "Phase-aware scheduling: " << (config.enable_phase_aware ? "ENABLED" : "DISABLED") << endl;

	// Create database with appropriate configuration
	DBConfig db_config;
	db_config.options.enable_phase_aware_scheduling = config.enable_phase_aware;
	DuckDB db(nullptr, &db_config);
	Connection con(db);

	// Setup TPC-H data
	SetupTPCHData(con);

	// Get performance logger
	auto &scheduler = TaskScheduler::GetScheduler(*db.instance);
	auto &logger = scheduler.GetPerformanceLogger();

	// Run queries
	for (int run = 0; run < config.num_runs; run++) {
		cout << "Run " << (run + 1) << "/" << config.num_runs << "..." << endl;

		for (const auto &query_name : config.queries) {
			const char *query_sql = nullptr;
			if (query_name == "Q1") {
				query_sql = TPCH_Q1;
			} else if (query_name == "Q6") {
				query_sql = TPCH_Q6;
			} else {
				cerr << "Unknown query: " << query_name << endl;
				continue;
			}

			auto result = RunQuery(con, query_name, query_sql, config.name, run, logger);
			results.push_back(result);

			cout << "  " << query_name << ": " << result.response_time_ms << " ms" << endl;
		}
	}
}

void ExportResults(const vector<ExperimentResult> &results, const string &filename) {
	ofstream file(filename);
	if (!file.is_open()) {
		cerr << "Failed to open file: " << filename << endl;
		return;
	}

	// CSV header
	file << "config_name,query_name,run_number,response_time_ms,total_tasks,elastic_tasks,inelastic_tasks\n";

	// Data rows
	for (const auto &result : results) {
		file << result.config_name << ","
		     << result.query_name << ","
		     << result.run_number << ","
		     << result.response_time_ms << ","
		     << result.total_tasks << ","
		     << result.elastic_tasks << ","
		     << result.inelastic_tasks << "\n";
	}

	file.close();
	cout << "\nResults exported to: " << filename << endl;
}

int main() {
	cout << "=== Phase-Aware Scheduler Experimental Data Generation ===" << endl;
	cout << "This program generates experimental data for the academic paper." << endl;

	vector<ExperimentResult> all_results;

	// Experiment 1: Baseline (Phase-aware OFF)
	ExperimentConfig baseline;
	baseline.name = "Phase0_Baseline";
	baseline.enable_phase_aware = false;
	baseline.num_runs = 5;
	baseline.queries = {"Q1", "Q6"};
	RunExperiment(baseline, all_results);

	// Experiment 2: Phase-aware ON
	ExperimentConfig phase_aware;
	phase_aware.name = "Phase1_PhaseAware";
	phase_aware.enable_phase_aware = true;
	phase_aware.num_runs = 5;
	phase_aware.queries = {"Q1", "Q6"};
	RunExperiment(phase_aware, all_results);

	// Export all results
	ExportResults(all_results, "experimental_results.csv");

	// Print summary
	cout << "\n=== Summary ===" << endl;
	cout << "Total experiments: 2" << endl;
	cout << "Total query executions: " << all_results.size() << endl;
	cout << "Results saved to: experimental_results.csv" << endl;

	return 0;
}
