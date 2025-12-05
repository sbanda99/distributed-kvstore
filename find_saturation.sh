#!/bin/bash

# Script to find saturation point by testing with increasing client counts
# Usage: ./find_saturation.sh <config_file> <protocol> <get_ratio> [duration]

# Configuration
CONFIG_FILE=$1
PROTOCOL=$2
GET_RATIO=$3
DURATION=${4:-60}  # Default to 60 seconds if not provided

if [ -z "$CONFIG_FILE" ] || [ -z "$PROTOCOL" ] || [ -z "$GET_RATIO" ]; then
    echo "Usage: $0 <config_file> <protocol> <get_ratio> [duration]"
    exit 1
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

if [ "$PROTOCOL" != "abd" ] && [ "$PROTOCOL" != "blocking" ]; then
    echo "Error: Protocol must be 'abd' or 'blocking'"
    exit 1
fi

# Extract number of servers from config file (count "id" fields in servers array)
NUM_SERVERS=$(grep -o '"id"' "$CONFIG_FILE" | wc -l)

echo "Finding Saturation Point"
echo "Config:      $CONFIG_FILE"
echo "Protocol:    $PROTOCOL"
echo "Get Ratio:   $GET_RATIO"
echo "Duration:    ${DURATION}s"
echo "Servers:     $NUM_SERVERS"
echo ""

# Create results directory with format: results_saturation_${num_servers}_${protocol}
RESULTS_DIR="results_saturation_${NUM_SERVERS}_${PROTOCOL}_${GET_RATIO}"
mkdir -p "$RESULTS_DIR"

# Test with different client counts
CLIENT_COUNTS=(20 30 40 50 60 70 80)

echo "Testing with client counts: ${CLIENT_COUNTS[*]}"
echo "Results will be saved to: $RESULTS_DIR/"
echo ""

for clients in "${CLIENT_COUNTS[@]}"; do
    echo "Testing with $clients clients..."
    
    OUTPUT_FILE="$RESULTS_DIR/results_${clients}clients.txt"
    TEMP_FILE="${OUTPUT_FILE}.tmp"
    
    # Run evaluation, redirect stderr to /dev/null to suppress verbose client logs
    # Only capture stdout (results)
    ./build/evaluate_performance "$CONFIG_FILE" "$PROTOCOL" "$clients" "$GET_RATIO" "$DURATION" \
        2>/dev/null > "$TEMP_FILE"
    
    # Extract only the performance results section (from "Performance Evaluation Results" to end)
    if [ -f "$TEMP_FILE" ]; then
        # Extract results section starting from "Performance Evaluation Results"
        awk '/Performance Evaluation Results/,0' "$TEMP_FILE" > "$OUTPUT_FILE"
        rm -f "$TEMP_FILE"
        
        # Extract key metrics for display
        THROUGHPUT=$(grep "Throughput:" "$OUTPUT_FILE" | grep -oE '[0-9]+\.[0-9]+' | head -1)
        TOTAL_OPS=$(grep "Total Operations:" "$OUTPUT_FILE" | grep -oE '[0-9]+')
        FAILED_OPS=$(grep "Failed Operations:" "$OUTPUT_FILE" | grep -oE '[0-9]+')
        
        echo "  Throughput: $THROUGHPUT ops/sec"
        echo "  Total Ops: $TOTAL_OPS"
        echo "  Failed Ops: $FAILED_OPS"
        echo "  Results saved to: $OUTPUT_FILE"
    else
        echo "  ERROR: Failed to generate results"
    fi
    
    echo ""
    sleep 1  # Brief pause between tests (reduced from 2 to 1)
done

echo "All tests complete!"
echo ""
echo "Summary of results:"
printf "%-10s %-15s %-15s %-15s\n" "Clients" "Throughput" "Total Ops" "Failed Ops"

for clients in "${CLIENT_COUNTS[@]}"; do
    OUTPUT_FILE="$RESULTS_DIR/results_${clients}clients.txt"
    if [ -f "$OUTPUT_FILE" ]; then
        THROUGHPUT=$(grep "Throughput:" "$OUTPUT_FILE" | grep -oE '[0-9]+\.[0-9]+' | head -1)
        TOTAL_OPS=$(grep "Total Operations:" "$OUTPUT_FILE" | grep -oE '[0-9]+')
        FAILED_OPS=$(grep "Failed Operations:" "$OUTPUT_FILE" | grep -oE '[0-9]+')
        
        if [ -n "$THROUGHPUT" ]; then
            printf "%-10s %-15s %-15s %-15s\n" "$clients" "$THROUGHPUT" "$TOTAL_OPS" "$FAILED_OPS"
        fi
    fi
done

echo ""
echo "Review detailed results in: $RESULTS_DIR/"
echo ""

