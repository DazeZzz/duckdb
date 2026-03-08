#!/bin/bash
# Experiment Monitor Script
# Usage: ./monitor_experiments.sh

WORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROGRESS_FILE="${WORK_DIR}/experiment_progress.log"
RESULTS_DIR="${WORK_DIR}/experiment_results"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

clear

echo -e "${BLUE}=== DuckDB Experiment Monitor ===${NC}"
echo ""

# Check if experiment is running
if ! pgrep -f "cloud_experiment_setup.sh" > /dev/null; then
    echo -e "${YELLOW}⚠ No experiment process found${NC}"
    echo ""
fi

# Show latest progress
if [ -f "$PROGRESS_FILE" ]; then
    echo -e "${GREEN}📊 Latest Progress:${NC}"
    tail -n 10 "$PROGRESS_FILE"
    echo ""

    # Extract current stage and percentage
    LAST_LINE=$(tail -n 1 "$PROGRESS_FILE")
    if [[ "$LAST_LINE" =~ \[([0-9]+)%\] ]]; then
        PERCENT="${BASH_REMATCH[1]}"
        echo -e "${BLUE}Progress: ${PERCENT}%${NC}"

        # Progress bar
        BAR_LENGTH=50
        FILLED=$((PERCENT * BAR_LENGTH / 100))
        printf "["
        printf "%${FILLED}s" | tr ' ' '='
        printf "%$((BAR_LENGTH - FILLED))s" | tr ' ' '-'
        printf "] ${PERCENT}%%\n"
        echo ""
    fi
else
    echo -e "${RED}✗ Progress file not found${NC}"
    echo ""
fi

# Check build status
echo -e "${GREEN}🔨 Build Status:${NC}"
for version in native stage2 stage3 final; do
    BUILD_PATH="${WORK_DIR}/builds/${version}"
    if [ -f "${BUILD_PATH}/duckdb" ] && [ -f "${BUILD_PATH}/test/api/comprehensive_experiment_runner" ]; then
        echo -e "  ${GREEN}✓${NC} ${version}: Built"
    elif [ -d "${BUILD_PATH}" ]; then
        echo -e "  ${YELLOW}⚠${NC} ${version}: Building..."
    else
        echo -e "  ${RED}✗${NC} ${version}: Not started"
    fi
done
echo ""

# Check TPC-H database
echo -e "${GREEN}💾 TPC-H Database:${NC}"
TPCH_DB="${WORK_DIR}/tpch_sf50.db"
if [ -f "$TPCH_DB" ]; then
    DB_SIZE=$(du -h "$TPCH_DB" | cut -f1)
    echo -e "  ${GREEN}✓${NC} Generated (${DB_SIZE})"
else
    echo -e "  ${RED}✗${NC} Not generated yet"
fi
echo ""

# Check experiment results
echo -e "${GREEN}📈 Experiment Results:${NC}"
if [ -d "$RESULTS_DIR" ]; then
    for version in native stage2 stage3 final; do
        RESULT_PATH="${RESULTS_DIR}/${version}"
        if [ -d "$RESULT_PATH" ]; then
            CSV_COUNT=$(find "$RESULT_PATH" -name "*.csv" -type f 2>/dev/null | wc -l)
            if [ "$CSV_COUNT" -gt 0 ]; then
                echo -e "  ${GREEN}✓${NC} ${version}: ${CSV_COUNT}/11 experiments completed"
            else
                echo -e "  ${YELLOW}⚠${NC} ${version}: Running..."
            fi
        else
            echo -e "  ${RED}✗${NC} ${version}: Not started"
        fi
    done
else
    echo -e "  ${RED}✗${NC} No results yet"
fi
echo ""

# System resources
echo -e "${GREEN}💻 System Resources:${NC}"
if command -v free &> /dev/null; then
    MEM_USED=$(free -g | awk '/^Mem:/{printf "%.1f", $3}')
    MEM_TOTAL=$(free -g | awk '/^Mem:/{print $2}')
    echo -e "  Memory: ${MEM_USED}GB / ${MEM_TOTAL}GB"
fi

CPU_LOAD=$(uptime | awk -F'load average:' '{print $2}' | awk '{print $1}')
echo -e "  CPU Load: ${CPU_LOAD}"

DISK_USAGE=$(df -h "$WORK_DIR" | awk 'NR==2 {print $5}')
echo -e "  Disk Usage: ${DISK_USAGE}"
echo ""

# Commands
echo -e "${BLUE}📝 Useful Commands:${NC}"
echo "  Watch live progress:  tail -f $PROGRESS_FILE"
echo "  View results:         ls -lh $RESULTS_DIR/*/*.csv"
echo "  Check processes:      ps aux | grep comprehensive_experiment_runner"
echo "  Refresh monitor:      watch -n 5 ./monitor_experiments.sh"
echo ""

# Auto-refresh option
if [ "$1" == "--watch" ]; then
    echo -e "${YELLOW}Auto-refreshing every 5 seconds... (Ctrl+C to stop)${NC}"
    sleep 5
    exec "$0" --watch
fi
