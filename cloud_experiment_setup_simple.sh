#!/bin/bash
# Cloud Experiment Setup Script - Simplified Version
# One-click script to build and run all experiments
# Usage: ./cloud_experiment_setup.sh

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}"
BUILD_DIR="${WORK_DIR}/build"
RESULTS_DIR="${WORK_DIR}/experiment_results_final"
PROGRESS_FILE="${WORK_DIR}/experiment_progress.log"
TPCH_DB="${WORK_DIR}/tpch_sf50.db"

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

# Build DuckDB and experiment runner
build_system() {
    log "Building DuckDB and experiment runner..."
    update_progress "BUILD" 10 "Starting build"

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Clean CMake cache
    if [ -f "CMakeCache.txt" ]; then
        rm -f CMakeCache.txt
        info "Cleaned CMake cache"
    fi

    # Configure with CMake
    info "Configuring CMake..."
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_UNITTESTS=0 \
          "$WORK_DIR" 2>&1 | tee -a "$PROGRESS_FILE"

    if [ ! -f "Makefile" ]; then
        error "CMake failed to generate Makefile"
    fi

    update_progress "BUILD" 20 "CMake configuration complete"

    # Build duckdb CLI
    info "Building duckdb CLI..."
    make -j$(nproc) duckdb 2>&1 | tee -a "$PROGRESS_FILE"

    if [ ! -f "duckdb" ]; then
        error "Failed to build duckdb binary"
    fi

    update_progress "BUILD" 30 "DuckDB CLI built successfully"

    # Build experiment runner
    info "Building comprehensive_experiment_runner..."
    make -j$(nproc) comprehensive_experiment_runner 2>&1 | tee -a "$PROGRESS_FILE"

    if [ ! -f "test/api/comprehensive_experiment_runner" ]; then
        error "Failed to build comprehensive_experiment_runner"
    fi

    info "✓ Build complete"
    update_progress "BUILD" 40 "All binaries built successfully"
}

# Generate TPC-H data
generate_tpch_data() {
    log "Generating TPC-H SF50 data..."
    update_progress "DATA" 40 "Generating TPC-H SF50 dataset"

    if [ -f "$TPCH_DB" ]; then
        warn "TPC-H database already exists, skipping generation"
        update_progress "DATA" 50 "Using existing TPC-H database"
        return
    fi

    # Use duckdb to generate data
    "$BUILD_DIR/duckdb" "$TPCH_DB" <<'EOF'
SET autoinstall_known_extensions=1;
SET autoload_known_extensions=1;
INSTALL tpch;
LOAD tpch;
CALL dbgen(sf=50);
SELECT 'TPC-H data generation completed' as status;
EOF

    # Verify database
    if [ ! -f "$TPCH_DB" ]; then
        error "Failed to generate TPC-H database"
    fi

    DB_SIZE=$(du -h "$TPCH_DB" | cut -f1)
    info "TPC-H database generated: $DB_SIZE"
    update_progress "DATA" 50 "TPC-H SF50 generated ($DB_SIZE)"
}

# Run experiments
run_experiments() {
    log "Running comprehensive experiments..."
    update_progress "EXPERIMENT" 50 "Starting all experiments"

    mkdir -p "$RESULTS_DIR"

    # Run experiment runner
    "$BUILD_DIR/test/api/comprehensive_experiment_runner" \
        "$TPCH_DB" \
        "$RESULTS_DIR" \
        2>&1 | while IFS= read -r line; do
            echo "$line" | tee -a "$PROGRESS_FILE"
            # Extract progress if available
            if [[ "$line" =~ "Experiment" ]]; then
                # Estimate progress based on experiment number (11 total)
                if [[ "$line" =~ "1.1" ]]; then
                    update_progress "EXPERIMENT" 55 "$line"
                elif [[ "$line" =~ "1.2" ]]; then
                    update_progress "EXPERIMENT" 60 "$line"
                elif [[ "$line" =~ "1.3" ]]; then
                    update_progress "EXPERIMENT" 65 "$line"
                elif [[ "$line" =~ "2.1" ]]; then
                    update_progress "EXPERIMENT" 70 "$line"
                elif [[ "$line" =~ "2.2" ]]; then
                    update_progress "EXPERIMENT" 75 "$line"
                elif [[ "$line" =~ "3.1" ]]; then
                    update_progress "EXPERIMENT" 80 "$line"
                elif [[ "$line" =~ "3.2" ]]; then
                    update_progress "EXPERIMENT" 85 "$line"
                elif [[ "$line" =~ "3.3" ]]; then
                    update_progress "EXPERIMENT" 90 "$line"
                elif [[ "$line" =~ "4.1" ]]; then
                    update_progress "EXPERIMENT" 93 "$line"
                elif [[ "$line" =~ "5.1" ]]; then
                    update_progress "EXPERIMENT" 96 "$line"
                elif [[ "$line" =~ "5.2" ]]; then
                    update_progress "EXPERIMENT" 99 "$line"
                fi
            fi
        done

    # Verify results
    CSV_COUNT=$(find "$RESULTS_DIR" -name "*.csv" -type f | wc -l)
    if [ "$CSV_COUNT" -eq 0 ]; then
        error "No CSV files generated"
    fi

    log "Experiments completed ($CSV_COUNT CSV files)"
    update_progress "EXPERIMENT" 100 "All experiments complete"
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

    # Step 2: Build system (10-40%)
    build_system

    # Step 3: Generate TPC-H data (40-50%)
    generate_tpch_data

    # Step 4: Run experiments (50-100%)
    run_experiments

    # Complete
    update_progress "COMPLETE" 100 "All experiments finished"
    log "=== Experiment Setup Complete ==="
    log "Results directory: $RESULTS_DIR"
    log "Progress log: $PROGRESS_FILE"

    # Summary
    echo ""
    echo "=== Summary ==="
    echo "Build: $BUILD_DIR"
    echo "Results: $RESULTS_DIR"
    echo "Progress: $PROGRESS_FILE"
    echo ""
    echo "Results contain experiments for 4 system versions:"
    echo "  - native: Original DuckDB"
    echo "  - stage2: Stride + Inelastic-First"
    echo "  - stage3: Stride + IF + Adaptive Morsel"
    echo "  - final: All 6 optimization phases"
    echo ""
    echo "Next steps:"
    echo "1. Check results: ls -lh $RESULTS_DIR/*.csv"
    echo "2. Download: scp -r user@server:$RESULTS_DIR ./"
}

# Run main function
main "$@"
