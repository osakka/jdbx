#!/bin/bash
#
# fix_format_warnings.sh
# Fixes format string warnings in the JSONdb codebase
#

echo "Starting to fix format string warnings..."

# JSONDB Metrics file - has several format string warnings
JSONDB_METRICS_FILE="src/tools/jsondb_metrics.c"

if [ -f "$JSONDB_METRICS_FILE" ]; then
    echo "Processing $JSONDB_METRICS_FILE..."

    # Fix format string warnings in analyze_metrics_file function
    # Change %llu to %lu for uint64_t types in sscanf calls
    sed -i 's/sscanf(line, "%255s counter value=%llu", name, &counters\[num_counters\].value)/sscanf(line, "%255s counter value=%lu", name, \&counters[num_counters].value)/g' "$JSONDB_METRICS_FILE"
    sed -i 's/sscanf(line, "%255s timer count=%llu min=%lf max=%lf sum=%lf avg=%lf", name, &timers\[num_timers\].count/sscanf(line, "%255s timer count=%lu min=%lf max=%lf sum=%lf avg=%lf", name, \&timers[num_timers].count/g' "$JSONDB_METRICS_FILE"
    sed -i 's/sscanf(line, "%255s gauge value=%llu", name, &gauges\[num_gauges\].value)/sscanf(line, "%255s gauge value=%lu", name, \&gauges[num_gauges].value)/g' "$JSONDB_METRICS_FILE"

    # Fix format string warnings in print functions
    # Change %llu to %lu for uint64_t types in fprintf/printf calls
    sed -i 's/fprintf(output, "%s counter value=%llu\\n", /fprintf(output, "%s counter value=%lu\\n", /g' "$JSONDB_METRICS_FILE"
    sed -i 's/fprintf(output, "%s timer count=%llu min=%.3f max=%.3f sum=%.3f avg=%.3f\\n", /fprintf(output, "%s timer count=%lu min=%.3f max=%.3f sum=%.3f avg=%.3f\\n", /g' "$JSONDB_METRICS_FILE"
    sed -i 's/fprintf(output, "%s gauge value=%llu\\n", /fprintf(output, "%s gauge value=%lu\\n", /g' "$JSONDB_METRICS_FILE"
    sed -i 's/printf("%s counter value=%llu\\n", /printf("%s counter value=%lu\\n", /g' "$JSONDB_METRICS_FILE"
    sed -i 's/printf("%s timer count=%llu min=%.3f max=%.3f sum=%.3f avg=%.3f\\n", /printf("%s timer count=%lu min=%.3f max=%.3f sum=%.3f avg=%.3f\\n", /g' "$JSONDB_METRICS_FILE"
    sed -i 's/printf("%s gauge value=%llu\\n", /printf("%s gauge value=%lu\\n", /g' "$JSONDB_METRICS_FILE"

    # Fix additional format warnings in HTML report generation
    sed -i 's/fprintf(output, "| %s | %llu | %.3f | %.3f | %.3f | %.3f |/fprintf(output, "| %s | %lu | %.3f | %.3f | %.3f | %.3f |/g' "$JSONDB_METRICS_FILE"
    sed -i 's/fprintf(output, "| %s | %llu | %.6f | %.6f | %.6f |/fprintf(output, "| %s | %lu | %.6f | %.6f | %.6f |/g' "$JSONDB_METRICS_FILE"

    # Fix for histogram counters
    sed -i 's/sscanf(line, "%255s histogram count=%llu min=%lf max=%lf sum=%lf avg=%lf"/sscanf(line, "%255s histogram count=%lu min=%lf max=%lf sum=%lf avg=%lf"/g' "$JSONDB_METRICS_FILE"

    # Fix remaining timer sscanf warnings in diff_metrics_files and other functions
    sed -i 's/sscanf(line, "%255s timer count=%llu min=%lf max=%lf sum=%lf avg=%lf", /sscanf(line, "%255s timer count=%lu min=%lf max=%lf sum=%lf avg=%lf", /g' "$JSONDB_METRICS_FILE"

    echo "Fixed format string warnings in $JSONDB_METRICS_FILE"
else
    echo "Error: $JSONDB_METRICS_FILE not found!"
fi

# Core metrics file - also has format string warnings
METRICS_FILE="src/utils/metrics.c"

if [ -f "$METRICS_FILE" ]; then
    echo "Processing $METRICS_FILE..."

    # Fix JSON format string warnings
    sed -i 's/"count":%llu,/"count":%lu,/g' "$METRICS_FILE"

    # Fix bucket format warnings
    sed -i 's/\\"count\\":%llu}/\\"count\\":%lu}/g' "$METRICS_FILE"
    sed -i 's/le":%f,"count":%llu}/le":%f,"count":%lu}/g' "$METRICS_FILE"

    # Fix fprintf format warnings
    sed -i 's/fprintf(file, "%s timer count=%llu min=%.6f max=%.6f sum=%.6f avg=%.6f\\n", /fprintf(file, "%s timer count=%lu min=%.6f max=%.6f sum=%.6f avg=%.6f\\n", /g' "$METRICS_FILE"
    sed -i 's/fprintf(file, "%s histogram count=%llu min=%.6f max=%.6f sum=%.6f avg=%.6f\\n", /fprintf(file, "%s histogram count=%lu min=%.6f max=%.6f sum=%.6f avg=%.6f\\n", /g' "$METRICS_FILE"
    sed -i 's/fprintf(file, "%s bucket le=%.6f count=%llu\\n", /fprintf(file, "%s bucket le=%.6f count=%lu\\n", /g' "$METRICS_FILE"

    # Fix remaining JSON outputs in snprintf calls
    sed -i 's/"count":%llu,"sum":%f/"count":%lu,"sum":%f/g' "$METRICS_FILE"
    sed -i 's/"count":%llu,"sum":%f,"min":%f,"max":%f,"mean":%f,"stddev":%f"/"count":%lu,"sum":%f,"min":%f,"max":%f,"mean":%f,"stddev":%f"/g' "$METRICS_FILE"
    sed -i 's/"count":%llu,"sum":%f,"min":%f,"max":%f,"buckets":\[/"count":%lu,"sum":%f,"min":%f,"max":%f,"buckets":\[/g' "$METRICS_FILE"

    # Direct fixes for the specific remaining format warnings we found
    sed -i 's/                        "\\\"count\\\":%llu,\\\"sum\\\":%f,\\\"min\\\":%f,\\\"max\\\":%f,\\\"mean\\\":%f,\\\"stddev\\\":%f",/                        "\\\"count\\\":%lu,\\\"sum\\\":%f,\\\"min\\\":%f,\\\"max\\\":%f,\\\"mean\\\":%f,\\\"stddev\\\":%f",/g' "$METRICS_FILE"
    sed -i 's/                    "\\\"count\\\":%llu,\\\"sum\\\":%f,\\\"min\\\":%f,\\\"max\\\":%f,\\\"buckets\\\":\[",/                    "\\\"count\\\":%lu,\\\"sum\\\":%f,\\\"min\\\":%f,\\\"max\\\":%f,\\\"buckets\\\":\[",/g' "$METRICS_FILE"

    echo "Fixed format string warnings in $METRICS_FILE"
else
    echo "Error: $METRICS_FILE not found!"
fi

# Check for any remaining potential format string warnings in the codebase
echo "Checking for remaining potential format string warnings..."
grep -r "%llu" --include="*.c" src/ | grep -v "long long" | grep -v "unsigned long long"

echo "Format string fix script completed."