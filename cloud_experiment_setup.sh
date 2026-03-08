#!/bin/bash
# Cloud Experiment Setup Script
# One-click script to build all versions and run experiments
# Usage: ./cloud_experiment_setup.sh

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}"
BUILD_DIR="${WORK_DIR}/builds"
RESULTS_DIR="${WORK_DIR}/experiment_results"
PROGRESS_FILE="${WORK_DIR}/experiment_progress.log"
TPCH_DB="${WORK_DIR}/tpch_sf50.db"

# Version definitions (commit hashes)
NATIVE_COMMIT="48bdbbebe4"  # Native DuckDB (main branch baseline)
STAGE2_COMMIT="7e5eb201ad"  # Phase 1: Stride + IF
STAGE3_COMMIT="cb54825ddb"  # Phase 2: Stride + IF + Morsel
FINAL_COMMIT="HEAD"          # Current HEAD with all phases

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Progress tracking
update_progress() {
    local stage=$1
    local percent=$2
    local message=$3
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$percent%] $stage: $message" >> "$PROGRESS_FILE"
    log "$stage ($percent%): $message"
}

# Check system requirements
check_requirements() {
    log "Checking system requirements..."

    # Check CPU cores
    CPU_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "unknown")
    info "CPU Cores: $CPU_CORES"

    # Check memory
    if command -v free &> /dev/null; then
        TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')
        info "Total Memory: ${TOTAL_MEM}GB"
    fi

    # Check required tools
    for cmd in git cmake make g++; do
        if ! command -v $cmd &> /dev/null; then
            error "$cmd is not installed"
        fi
    done

    update_progress "INIT" 0 "System check passed"
}

# Build a specific version
build_version() {
    local version_name=$1
    local commit_hash=$2
    local build_path="${BUILD_DIR}/${version_name}"

    log "Building $version_name (commit: $commit_hash)..."
    update_progress "BUILD" $((10 + ${3:-0})) "Building $version_name"

    # Create build directory
    mkdir -p "$build_path"

    # Save current branch
    CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

    # Save the comprehensive_experiment_runner.cpp (we'll restore this)
    EXPERIMENT_RUNNER="${WORK_DIR}/test/api/comprehensive_experiment_runner.cpp"

    if [ -f "$EXPERIMENT_RUNNER" ]; then
        cp "$EXPERIMENT_RUNNER" /tmp/comprehensive_experiment_runner.cpp.backup
    fi

    # Checkout specific commit
    git checkout "$commit_hash" 2>&1 | tee -a "$PROGRESS_FILE"

    # Force restore CMakeLists.txt to clean state from the commit
    CMAKE_FILE="${WORK_DIR}/test/api/CMakeLists.txt"
    git checkout HEAD -- test/api/CMakeLists.txt 2>&1 | tee -a "$PROGRESS_FILE"
    info "Restored CMakeLists.txt to clean state from commit"

    # Restore comprehensive_experiment_runner.cpp
    if [ -f /tmp/comprehensive_experiment_runner.cpp.backup ]; then
        cp /tmp/comprehensive_experiment_runner.cpp.backup "${WORK_DIR}/test/api/comprehensive_experiment_runner.cpp"
        info "Restored comprehensive_experiment_runner.cpp"
        # Verify the file exists and has content
        if [ -s "${WORK_DIR}/test/api/comprehensive_experiment_runner.cpp" ]; then
            FILE_SIZE=$(wc -c < "${WORK_DIR}/test/api/comprehensive_experiment_runner.cpp")
            info "✓ Verified: comprehensive_experiment_runner.cpp exists (${FILE_SIZE} bytes)"
        else
            error "✗ comprehensive_experiment_runner.cpp is missing or empty!"
        fi
    fi

    # Add comprehensive_experiment_runner target to CMakeLists.txt
    info "Adding comprehensive_experiment_runner target to CMakeLists.txt"
    cat >> "$CMAKE_FILE" << 'EOF'

# Comprehensive Experiment Runner for Phase-Aware Scheduler
add_executable(comprehensive_experiment_runner comprehensive_experiment_runner.cpp)
target_link_libraries(comprehensive_experiment_runner duckdb_static)
link_extension_libraries(comprehensive_experiment_runner "")
EOF
    info "✓ Added comprehensive_experiment_runner target to CMakeLists.txt"

    # Build out-of-source (keeps source directory clean)
    mkdir -p "$build_path"
    cd "$build_path"

    # Clean CMake cache to ensure it picks up the restored files
    if [ -f "CMakeCache.txt" ]; then
        rm -f CMakeCache.txt
        info "Cleaned CMake cache to force reconfiguration"
    fi

    # Configure with CMake (source dir is $WORK_DIR, build dir is current)
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_UNITTESTS=0 \
          "$WORK_DIR" 2>&1 | tee -a "$PROGRESS_FILE"

    # Build duckdb CLI
    make -j$(nproc) duckdb 2>&1 | tee -a "$PROGRESS_FILE"

    # Build experiment runner
    make -j$(nproc) comprehensive_experiment_runner 2>&1 | tee -a "$PROGRESS_FILE"

    # Verify binary exists
    info "Checking for binaries..."

    # Check for duckdb binary
    if [ -f "$build_path/duckdb" ]; then
        info "✓ duckdb binary found at $build_path/duckdb"
    else
        error "Failed to build $version_name: duckdb binary not found at $build_path/duckdb"
    fi

    # Check for experiment runner
    if [ -f "$build_path/test/api/comprehensive_experiment_runner" ]; then
        info "✓ comprehensive_experiment_runner found"
    else
        error "Failed to build $version_name: experiment runner not found"
    fi

    # Return to original branch
    cd "$WORK_DIR"
    git checkout "$CURRENT_BRANCH"

    log "$version_name built successfully"
    update_progress "BUILD" $((15 + ${3:-0})) "$version_name build complete"
}

# Generate TPC-H data
generate_tpch_data() {
    log "Generating TPC-H SF50 data..."
    update_progress "DATA" 40 "Generating TPC-H SF50 dataset"

    if [ -f "$TPCH_DB" ]; then
        warn "TPC-H database already exists, skipping generation"
        return
    fi

    # Use native version to generate data
    "${BUILD_DIR}/native/duckdb" "$TPCH_DB" <<'EOF'
SET autoinstall_known_extensions=1;
SET autoload_known_extensions=1;
INSTALL tpch;
LOAD tpch;
CALL dbgen(sf=50);
SELECT 'TPC-H data generation completed' as status;
EOF

    # Verify database
    DB_SIZE=$(du -h "$TPCH_DB" | cut -f1)
    info "TPC-H database generated: $DB_SIZE"
    update_progress "DATA" 50 "TPC-H SF50 generated ($DB_SIZE)"
}

# Run experiments for a specific version
run_experiments() {
    local version_name=$1
    local build_path="${BUILD_DIR}/${version_name}"
    local results_path="${RESULTS_DIR}/${version_name}"
    local progress_percent=$2

    log "Running experiments for $version_name..."
    update_progress "EXPERIMENT" $progress_percent "Starting experiments for $version_name"

    mkdir -p "$results_path"

    # Run experiment runner with progress monitoring
    "${build_path}/test/api/comprehensive_experiment_runner" \
        "$TPCH_DB" \
        "$results_path" \
        2>&1 | while IFS= read -r line; do
            echo "$line" | tee -a "$PROGRESS_FILE"
            # Extract progress if available
            if [[ "$line" =~ "Experiment" ]]; then
                update_progress "EXPERIMENT" $progress_percent "$version_name: $line"
            fi
        done

    # Verify results
    CSV_COUNT=$(find "$results_path" -name "*.csv" -type f | wc -l)
    if [ "$CSV_COUNT" -eq 0 ]; then
        error "No CSV files generated for $version_name"
    fi

    log "$version_name experiments completed ($CSV_COUNT CSV files)"
    update_progress "EXPERIMENT" $((progress_percent + 10)) "$version_name experiments complete"
}

# Main execution
main() {
    log "=== DuckDB Phase-Aware Scheduler Experiment Setup ==="
    log "Work Directory: $WORK_DIR"

    # Initialize
    mkdir -p "$BUILD_DIR" "$RESULTS_DIR"
    echo "=== Experiment Progress Log ===" > "$PROGRESS_FILE"
    echo "Started at: $(date)" >> "$PROGRESS_FILE"

    # Step 1: Check requirements (0-10%)
    check_requirements

    # Step 2: Build all versions (10-40%)
    log "Building all versions..."
    build_version "native" "$NATIVE_COMMIT" 0
    build_version "stage2" "$STAGE2_COMMIT" 10
    build_version "stage3" "$STAGE3_COMMIT" 20
    build_version "final" "$FINAL_COMMIT" 30

    # Step 3: Generate TPC-H data (40-50%)
    generate_tpch_data

    # Step 4: Run experiments (50-100%)
    log "Running experiments for all versions..."
    run_experiments "native" 50
    run_experiments "stage2" 65
    run_experiments "stage3" 80
    run_experiments "final" 95

    # Complete
    update_progress "COMPLETE" 100 "All experiments finished"
    log "=== Experiment Setup Complete ==="
    log "Results directory: $RESULTS_DIR"
    log "Progress log: $PROGRESS_FILE"

    # Summary
    echo ""
    echo "=== Summary ==="
    echo "Builds: $BUILD_DIR"
    echo "Results: $RESULTS_DIR"
    echo "Progress: $PROGRESS_FILE"
    echo ""
    echo "Next steps:"
    echo "1. Check progress: tail -f $PROGRESS_FILE"
    echo "2. View results: ls -lh $RESULTS_DIR/*/*.csv"
    echo "3. Analyze data: python3 analyze_results.py"
}

# Run main function
main "$@"
