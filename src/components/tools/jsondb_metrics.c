#include "utils/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("Usage: %s <command> [options]\n", program_name);
    printf("\n");
    printf("Commands:\n");
    printf("  analyze <file> [--output=<file>]\n");
    printf("    Analyze a metrics file and generate a report\n");
    printf("\n");
    printf("  diff <file1> <file2> [--output=<file>]\n");
    printf("    Compare two metrics files and show differences\n");
    printf("\n");
    printf("  report <dir> [--output=<file>] [--days=<num>]\n");
    printf("    Generate a performance report from metrics files\n");
    printf("\n");
    printf("  export <db_path> [--output=<file>]\n");
    printf("    Export current metrics from a running database\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help, -h                   Show this help message\n");
    printf("  --output=<file>, -o <file>   Output file (default: stdout)\n");
    printf("  --days=<num>, -d <num>       Number of days to include (default: 7)\n");
}

/* Metric data structure for analysis */
typedef struct {
    char name[256];
    char type[32];
    double value;
    uint64_t count;
    double min;
    double max;
    double sum;
    double avg;
} analyzed_metric_t;

/* Analyze a metrics file */
static int analyze_metrics_file(const char* filename, const char* output_file) {
    FILE* input = fopen(filename, "r");
    if (!input) {
        fprintf(stderr, "Error: Cannot open input file: %s\n", filename);
        return 1;
    }
    
    FILE* output = stdout;
    if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
            fclose(input);
            return 1;
        }
    }
    
    /* Read and analyze metrics */
    char line[1024];
    char timestamp[64] = "";
    int line_num = 0;
    
    /* Arrays to store metrics by type */
    analyzed_metric_t counters[100];
    int num_counters = 0;
    
    analyzed_metric_t gauges[100];
    int num_gauges = 0;
    
    analyzed_metric_t timers[100];
    int num_timers = 0;
    
    analyzed_metric_t histograms[100];
    int num_histograms = 0;
    
    /* Process each line of the metrics file */
    while (fgets(line, sizeof(line), input)) {
        line_num++;
        
        /* Skip empty lines */
        if (line[0] == '\n') continue;
        
        /* Extract timestamp from the header */
        if (line[0] == '#' && strstr(line, "JSON Database Metrics")) {
            char* ts_start = strstr(line, "- ");
            if (ts_start) {
                ts_start += 2;
                char* ts_end = strchr(ts_start, '\n');
                if (ts_end) {
                    *ts_end = '\0';
                    strcpy(timestamp, ts_start);
                }
            }
            continue;
        }
        
        /* Skip other comments */
        if (line[0] == '#') continue;
        
        /* Parse metric line */
        char name[256];
        
        /* Try to parse different metric formats */
        if (sscanf(line, "%255s counter %lf", name, &counters[num_counters].value) == 2) {
            strcpy(counters[num_counters].name, name);
            strcpy(counters[num_counters].type, "counter");
            num_counters++;
        }
        else if (sscanf(line, "%255s gauge %lf", name, &gauges[num_gauges].value) == 2) {
            strcpy(gauges[num_gauges].name, name);
            strcpy(gauges[num_gauges].type, "gauge");
            num_gauges++;
        }
        else if (sscanf(line, "%255s timer count=%lu min=%lf max=%lf sum=%lf avg=%lf", 
                       name, &timers[num_timers].count, &timers[num_timers].min, 
                       &timers[num_timers].max, &timers[num_timers].sum, &timers[num_timers].avg) == 6) {
            strcpy(timers[num_timers].name, name);
            strcpy(timers[num_timers].type, "timer");
            num_timers++;
        }
        else if (sscanf(line, "%255s histogram count=%lu min=%lf max=%lf sum=%lf avg=%lf", 
                       name, &histograms[num_histograms].count, &histograms[num_histograms].min, 
                       &histograms[num_histograms].max, &histograms[num_histograms].sum, 
                       &histograms[num_histograms].avg) == 6) {
            strcpy(histograms[num_histograms].name, name);
            strcpy(histograms[num_histograms].type, "histogram");
            num_histograms++;
        }
        /* Skip bucket lines for now */
    }
    
    /* Close input file */
    fclose(input);
    
    /* Generate report */
    fprintf(output, "# Metrics Analysis Report\n");
    fprintf(output, "File: %s\n", filename);
    fprintf(output, "Timestamp: %s\n", timestamp);
    fprintf(output, "\n");
    
    /* Counters */
    fprintf(output, "## Counters (%d)\n", num_counters);
    fprintf(output, "| Name | Value |\n");
    fprintf(output, "|------|-------|\n");
    
    for (int i = 0; i < num_counters; i++) {
        fprintf(output, "| %s | %.0f |\n", counters[i].name, counters[i].value);
    }
    
    fprintf(output, "\n");
    
    /* Gauges */
    fprintf(output, "## Gauges (%d)\n", num_gauges);
    fprintf(output, "| Name | Value |\n");
    fprintf(output, "|------|-------|\n");
    
    for (int i = 0; i < num_gauges; i++) {
        fprintf(output, "| %s | %.6f |\n", gauges[i].name, gauges[i].value);
    }
    
    fprintf(output, "\n");
    
    /* Timers */
    fprintf(output, "## Timers (%d)\n", num_timers);
    fprintf(output, "| Name | Count | Min (ms) | Max (ms) | Avg (ms) | Total (ms) |\n");
    fprintf(output, "|------|-------|----------|----------|----------|------------|\n");
    
    for (int i = 0; i < num_timers; i++) {
        fprintf(output, "| %s | %lu | %.3f | %.3f | %.3f | %.3f |\n", 
               timers[i].name, 
               timers[i].count, 
               timers[i].min * 1000, 
               timers[i].max * 1000, 
               timers[i].avg * 1000, 
               timers[i].sum * 1000);
    }
    
    fprintf(output, "\n");
    
    /* Histograms */
    fprintf(output, "## Histograms (%d)\n", num_histograms);
    fprintf(output, "| Name | Count | Min | Max | Avg |\n");
    fprintf(output, "|------|-------|-----|-----|-----|\n");
    
    for (int i = 0; i < num_histograms; i++) {
        fprintf(output, "| %s | %lu | %.6f | %.6f | %.6f |\n", 
               histograms[i].name, 
               histograms[i].count, 
               histograms[i].min, 
               histograms[i].max, 
               histograms[i].avg);
    }
    
    /* Close output file if not stdout */
    if (output != stdout) {
        fclose(output);
    }
    
    return 0;
}

/* Compare two metrics files */
static int diff_metrics_files(const char* file1, const char* file2, const char* output_file) {
    FILE* input1 = fopen(file1, "r");
    if (!input1) {
        fprintf(stderr, "Error: Cannot open first input file: %s\n", file1);
        return 1;
    }
    
    FILE* input2 = fopen(file2, "r");
    if (!input2) {
        fprintf(stderr, "Error: Cannot open second input file: %s\n", file2);
        fclose(input1);
        return 1;
    }
    
    FILE* output = stdout;
    if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
            fclose(input1);
            fclose(input2);
            return 1;
        }
    }
    
    /* Simple implementation that reads all metrics into memory */
    /* In a real implementation, you'd want to use a more efficient data structure */
    
    /* Read first file metrics */
    char line[1024];
    char timestamp1[64] = "";
    
    /* Arrays to store metrics from first file */
    analyzed_metric_t counters1[100];
    int num_counters1 = 0;
    
    analyzed_metric_t gauges1[100];
    int num_gauges1 = 0;
    
    analyzed_metric_t timers1[100];
    int num_timers1 = 0;
    
    /* Process each line of the first metrics file */
    while (fgets(line, sizeof(line), input1)) {
        /* Extract timestamp from the header */
        if (line[0] == '#' && strstr(line, "JSON Database Metrics")) {
            char* ts_start = strstr(line, "- ");
            if (ts_start) {
                ts_start += 2;
                char* ts_end = strchr(ts_start, '\n');
                if (ts_end) {
                    *ts_end = '\0';
                    strcpy(timestamp1, ts_start);
                }
            }
            continue;
        }
        
        /* Skip other comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;
        
        /* Parse metric line */
        char name[256];
        
        /* Try to parse different metric formats */
        if (sscanf(line, "%255s counter %lf", name, &counters1[num_counters1].value) == 2) {
            strcpy(counters1[num_counters1].name, name);
            strcpy(counters1[num_counters1].type, "counter");
            num_counters1++;
        }
        else if (sscanf(line, "%255s gauge %lf", name, &gauges1[num_gauges1].value) == 2) {
            strcpy(gauges1[num_gauges1].name, name);
            strcpy(gauges1[num_gauges1].type, "gauge");
            num_gauges1++;
        }
        else if (sscanf(line, "%255s timer count=%lu min=%lf max=%lf sum=%lf avg=%lf", 
                       name, &timers1[num_timers1].count, &timers1[num_timers1].min, 
                       &timers1[num_timers1].max, &timers1[num_timers1].sum, &timers1[num_timers1].avg) == 6) {
            strcpy(timers1[num_timers1].name, name);
            strcpy(timers1[num_timers1].type, "timer");
            num_timers1++;
        }
    }
    
    /* Read second file metrics */
    char timestamp2[64] = "";
    
    /* Arrays to store metrics from second file */
    analyzed_metric_t counters2[100];
    int num_counters2 = 0;
    
    analyzed_metric_t gauges2[100];
    int num_gauges2 = 0;
    
    analyzed_metric_t timers2[100];
    int num_timers2 = 0;
    
    /* Process each line of the second metrics file */
    rewind(input2);
    while (fgets(line, sizeof(line), input2)) {
        /* Extract timestamp from the header */
        if (line[0] == '#' && strstr(line, "JSON Database Metrics")) {
            char* ts_start = strstr(line, "- ");
            if (ts_start) {
                ts_start += 2;
                char* ts_end = strchr(ts_start, '\n');
                if (ts_end) {
                    *ts_end = '\0';
                    strcpy(timestamp2, ts_start);
                }
            }
            continue;
        }
        
        /* Skip other comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;
        
        /* Parse metric line */
        char name[256];
        
        /* Try to parse different metric formats */
        if (sscanf(line, "%255s counter %lf", name, &counters2[num_counters2].value) == 2) {
            strcpy(counters2[num_counters2].name, name);
            strcpy(counters2[num_counters2].type, "counter");
            num_counters2++;
        }
        else if (sscanf(line, "%255s gauge %lf", name, &gauges2[num_gauges2].value) == 2) {
            strcpy(gauges2[num_gauges2].name, name);
            strcpy(gauges2[num_gauges2].type, "gauge");
            num_gauges2++;
        }
        else if (sscanf(line, "%255s timer count=%lu min=%lf max=%lf sum=%lf avg=%lf", 
                       name, &timers2[num_timers2].count, &timers2[num_timers2].min, 
                       &timers2[num_timers2].max, &timers2[num_timers2].sum, &timers2[num_timers2].avg) == 6) {
            strcpy(timers2[num_timers2].name, name);
            strcpy(timers2[num_timers2].type, "timer");
            num_timers2++;
        }
    }
    
    /* Close input files */
    fclose(input1);
    fclose(input2);
    
    /* Generate comparison report */
    fprintf(output, "# Metrics Comparison Report\n");
    fprintf(output, "Before: %s (%s)\n", file1, timestamp1);
    fprintf(output, "After: %s (%s)\n", file2, timestamp2);
    fprintf(output, "\n");
    
    /* Compare counters */
    fprintf(output, "## Counter Differences\n");
    fprintf(output, "| Name | Before | After | Change | %% Change |\n");
    fprintf(output, "|------|--------|-------|--------|----------|\n");
    
    for (int i = 0; i < num_counters2; i++) {
        /* Find matching counter in first file */
        int found = 0;
        double before_value = 0;
        
        for (int j = 0; j < num_counters1; j++) {
            if (strcmp(counters2[i].name, counters1[j].name) == 0) {
                found = 1;
                before_value = counters1[j].value;
                break;
            }
        }
        
        double after_value = counters2[i].value;
        double change = after_value - before_value;
        double pct_change = found ? (change / before_value) * 100 : 0;
        
        if (!found) {
            fprintf(output, "| %s | N/A | %.0f | N/A | N/A |\n", 
                   counters2[i].name, after_value);
        } else if (change != 0) {
            fprintf(output, "| %s | %.0f | %.0f | %.0f | %.2f%% |\n", 
                   counters2[i].name, before_value, after_value, change, pct_change);
        }
    }
    
    /* Check for counters in first file but not in second */
    for (int i = 0; i < num_counters1; i++) {
        int found = 0;
        
        for (int j = 0; j < num_counters2; j++) {
            if (strcmp(counters1[i].name, counters2[j].name) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            fprintf(output, "| %s | %.0f | N/A | N/A | N/A |\n", 
                   counters1[i].name, counters1[i].value);
        }
    }
    
    fprintf(output, "\n");
    
    /* Compare gauges */
    fprintf(output, "## Gauge Differences\n");
    fprintf(output, "| Name | Before | After | Change | %% Change |\n");
    fprintf(output, "|------|--------|-------|--------|----------|\n");
    
    for (int i = 0; i < num_gauges2; i++) {
        /* Find matching gauge in first file */
        int found = 0;
        double before_value = 0;
        
        for (int j = 0; j < num_gauges1; j++) {
            if (strcmp(gauges2[i].name, gauges1[j].name) == 0) {
                found = 1;
                before_value = gauges1[j].value;
                break;
            }
        }
        
        double after_value = gauges2[i].value;
        double change = after_value - before_value;
        double pct_change = found ? (change / before_value) * 100 : 0;
        
        if (!found) {
            fprintf(output, "| %s | N/A | %.6f | N/A | N/A |\n", 
                   gauges2[i].name, after_value);
        } else if (change != 0) {
            fprintf(output, "| %s | %.6f | %.6f | %.6f | %.2f%% |\n", 
                   gauges2[i].name, before_value, after_value, change, pct_change);
        }
    }
    
    /* Check for gauges in first file but not in second */
    for (int i = 0; i < num_gauges1; i++) {
        int found = 0;
        
        for (int j = 0; j < num_gauges2; j++) {
            if (strcmp(gauges1[i].name, gauges2[j].name) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            fprintf(output, "| %s | %.6f | N/A | N/A | N/A |\n", 
                   gauges1[i].name, gauges1[i].value);
        }
    }
    
    fprintf(output, "\n");
    
    /* Compare timers */
    fprintf(output, "## Timer Differences (Average ms)\n");
    fprintf(output, "| Name | Before | After | Change | %% Change |\n");
    fprintf(output, "|------|--------|-------|--------|----------|\n");
    
    for (int i = 0; i < num_timers2; i++) {
        /* Find matching timer in first file */
        int found = 0;
        double before_avg = 0;
        
        for (int j = 0; j < num_timers1; j++) {
            if (strcmp(timers2[i].name, timers1[j].name) == 0) {
                found = 1;
                before_avg = timers1[j].avg;
                break;
            }
        }
        
        double after_avg = timers2[i].avg;
        double change = after_avg - before_avg;
        double pct_change = found ? (change / before_avg) * 100 : 0;
        
        if (!found) {
            fprintf(output, "| %s | N/A | %.3f | N/A | N/A |\n", 
                   timers2[i].name, after_avg * 1000);
        } else if (change != 0) {
            fprintf(output, "| %s | %.3f | %.3f | %.3f | %.2f%% |\n", 
                   timers2[i].name, before_avg * 1000, after_avg * 1000, 
                   change * 1000, pct_change);
        }
    }
    
    /* Check for timers in first file but not in second */
    for (int i = 0; i < num_timers1; i++) {
        int found = 0;
        
        for (int j = 0; j < num_timers2; j++) {
            if (strcmp(timers1[i].name, timers2[j].name) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            fprintf(output, "| %s | %.3f | N/A | N/A | N/A |\n", 
                   timers1[i].name, timers1[i].avg * 1000);
        }
    }
    
    /* Close output file if not stdout */
    if (output != stdout) {
        fclose(output);
    }
    
    return 0;
}

/* Generate a performance report from metrics files */
static int generate_report(const char* dir_path, const char* output_file, int days) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory: %s\n", dir_path);
        return 1;
    }
    
    FILE* output = stdout;
    if (output_file) {
        output = fopen(output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
            closedir(dir);
            return 1;
        }
    }
    
    /* Generate report header */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(output, "# JSON Database Performance Report\n");
    fprintf(output, "Generated: %s\n", timestamp);
    fprintf(output, "Period: Last %d days\n", days);
    fprintf(output, "\n");
    
    /* Filter metrics files by age and sort by date */
    /* This is a simplified version that just lists files */
    
    fprintf(output, "## Available Metrics Files\n");
    fprintf(output, "\n");
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, "metrics_") != NULL) {
            fprintf(output, "- %s\n", entry->d_name);
        }
    }
    
    fprintf(output, "\n");
    fprintf(output, "## Summary\n");
    fprintf(output, "\n");
    fprintf(output, "For a complete analysis, use the 'analyze' command on individual metrics files.\n");
    
    /* Close directory */
    closedir(dir);
    
    /* Close output file if not stdout */
    if (output != stdout) {
        fclose(output);
    }
    
    return 0;
}

/* Export metrics from a running database */
static int export_metrics(const char* db_path __attribute__((unused)), const char* output_file __attribute__((unused))) {
    /* In a real implementation, this would connect to the database via IPC or API */
    fprintf(stderr, "Error: Metrics export from a running database is not implemented in this demo.\n");
    return 1;
}

/* Main function */
int main(int argc, char** argv) {
    /* Check arguments */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Get command */
    const char* command = argv[1];
    
    /* Parse command-specific options */
    if (strcmp(command, "analyze") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing argument for analyze command\n");
            return 1;
        }
        
        const char* filename = argv[2];
        const char* output_file = NULL;
        
        static struct option long_options[] = {
            {"output", required_argument, 0, 'o'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        optind = 2;  /* Start parsing from the second argument */
        
        while ((opt = getopt_long(argc, argv, "o:h", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'o':
                    output_file = optarg;
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    fprintf(stderr, "Error: Unknown option\n");
                    return 1;
            }
        }
        
        return analyze_metrics_file(filename, output_file);
    }
    else if (strcmp(command, "diff") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: Missing arguments for diff command\n");
            return 1;
        }
        
        const char* file1 = argv[2];
        const char* file2 = argv[3];
        const char* output_file = NULL;
        
        static struct option long_options[] = {
            {"output", required_argument, 0, 'o'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        optind = 3;  /* Start parsing from the third argument */
        
        while ((opt = getopt_long(argc, argv, "o:h", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'o':
                    output_file = optarg;
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    fprintf(stderr, "Error: Unknown option\n");
                    return 1;
            }
        }
        
        return diff_metrics_files(file1, file2, output_file);
    }
    else if (strcmp(command, "report") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing argument for report command\n");
            return 1;
        }
        
        const char* dir_path = argv[2];
        const char* output_file = NULL;
        int days = 7;  /* Default to 7 days */
        
        static struct option long_options[] = {
            {"output", required_argument, 0, 'o'},
            {"days", required_argument, 0, 'd'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        optind = 2;  /* Start parsing from the second argument */
        
        while ((opt = getopt_long(argc, argv, "o:d:h", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'o':
                    output_file = optarg;
                    break;
                case 'd':
                    days = atoi(optarg);
                    if (days <= 0) {
                        fprintf(stderr, "Error: Days must be a positive number\n");
                        return 1;
                    }
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    fprintf(stderr, "Error: Unknown option\n");
                    return 1;
            }
        }
        
        return generate_report(dir_path, output_file, days);
    }
    else if (strcmp(command, "export") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing argument for export command\n");
            return 1;
        }
        
        const char* db_path = argv[2];
        const char* output_file = NULL;
        
        static struct option long_options[] = {
            {"output", required_argument, 0, 'o'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        optind = 2;  /* Start parsing from the second argument */
        
        while ((opt = getopt_long(argc, argv, "o:h", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'o':
                    output_file = optarg;
                    break;
                case 'h':
                    print_usage(argv[0]);
                    return 0;
                default:
                    fprintf(stderr, "Error: Unknown option\n");
                    return 1;
            }
        }
        
        return export_metrics(db_path, output_file);
    }
    else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}