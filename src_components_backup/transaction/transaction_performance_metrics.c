#include "jsondb/transaction/transaction.h"
#include "jsondb/utils/json.h"
#include "jsondb/utils/json_helpers.h"
#include "jsondb/utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>

/* Implementation of transaction_get_detailed_performance_metrics - different name to avoid conflict */
json_value_t* transaction_get_detailed_performance_metrics(transaction_manager_t* manager, const char* metric_type, time_t start_time, time_t end_time) {
    LOG_INFO("Generating detailed performance metrics: type=%s, start_time=%ld, end_time=%ld",
             metric_type ? metric_type : "performance", (long)start_time, (long)end_time);

    if (!manager || !manager->log) {
        LOG_ERROR("Invalid transaction manager or log is NULL");
        return NULL;
    }

    /* Create a metrics container */
    json_value_t* metrics = json_create_object();
    if (!metrics) {
        return NULL;
    }

    /* Set metric type */
    json_object_set(metrics, "metric_type", json_create_string(metric_type ? metric_type : "performance"));

    /* Create buckets for time-based metrics */
    json_value_t* time_buckets = json_create_array();
    if (!time_buckets) {
        json_free(metrics);
        return NULL;
    }
    json_object_set(metrics, "time_series", time_buckets);

    /* Create overall statistics */
    json_value_t* overall = json_create_object();
    if (!overall) {
        json_free(metrics);
        return NULL;
    }
    json_object_set(metrics, "overall", overall);

    /* Use the transaction log to gather metrics */
    transaction_log_t* log = manager->log;

    pthread_mutex_lock(&log->lock);

    /* Calculate the number of buckets (hourly) */
    int num_buckets = (end_time - start_time) / 3600 + 1;
    if (num_buckets <= 0) {
        num_buckets = 1;
    }

    /* Initialize overall counters */
    int total_transactions = 0;
    int committed_transactions = 0;
    int aborted_transactions = 0;
    int64_t total_duration = 0;
    int64_t min_duration = LONG_MAX;
    int64_t max_duration = 0;

    /* Create and initialize buckets */
    for (int i = 0; i < num_buckets; i++) {
        json_value_t* bucket = json_create_object();
        if (!bucket) {
            continue;
        }

        time_t bucket_time = start_time + (i * 3600);
        json_object_set(bucket, "timestamp", json_create_integer(bucket_time));
        json_object_set(bucket, "transactions", json_create_integer(0));
        json_object_set(bucket, "committed", json_create_integer(0));
        json_object_set(bucket, "aborted", json_create_integer(0));
        json_object_set(bucket, "avg_duration", json_create_integer(0));

        json_array_append(time_buckets, bucket);
    }

    /* Check if we need to scan the log file or can use the in-memory metrics */
    int use_in_memory_metrics = 0;

    /* Simple case: if both start_time and end_time are 0 (get all metrics),
       or if the range includes current time, use the in-memory metrics */
    if ((start_time == 0 && end_time == 0) ||
        (end_time >= log->last_metrics_update)) {
        use_in_memory_metrics = 1;
    }

    /* If we can use in-memory metrics, populate from there */
    if (use_in_memory_metrics) {
        /* Use the pre-calculated metrics in the log structure */
        total_transactions = (int)log->total_transactions;
        committed_transactions = (int)log->committed_transactions;
        aborted_transactions = (int)log->aborted_transactions;

        /* Simple case - only one bucket covering current time */
        if (num_buckets == 1) {
            /* Get the bucket */
            json_value_t* bucket = json_array_get(time_buckets, 0);
            if (bucket) {
                /* Set the metrics */
                json_object_set(bucket, "transactions", json_create_integer(total_transactions));
                json_object_set(bucket, "committed", json_create_integer(committed_transactions));
                json_object_set(bucket, "aborted", json_create_integer(aborted_transactions));
                json_object_set(bucket, "avg_duration", json_create_integer(log->avg_duration_ms / 1000)); /* Convert to seconds */
            }
        }

        /* For more detailed buckets, we still need to scan the log */
    }

    /* If not using in-memory metrics, or if we need detailed buckets, scan the log file */
    if (!use_in_memory_metrics || num_buckets > 1) {
        /* Open the log file */
        FILE* file = fopen(log->log_file, "r");
        if (!file) {
            pthread_mutex_unlock(&log->lock);
            json_free(metrics);
            return NULL;
        }

        /* Process the log file to gather metrics */
        char line[4096];

        /* Track transactions by ID */
        json_value_t* tx_map = json_create_object();

        /* Process the log file */
        while (fgets(line, sizeof(line), file)) {
            /* Skip comments and empty lines */
            if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') {
                continue;
            }

            /* Parse log entry: TIMESTAMP|TYPE|TRANSACTION_ID|DATA_JSON */
            char* line_copy = strdup(line);
            if (!line_copy) continue;

            char* token = strtok(line_copy, "|");
            if (!token) {
                free(line_copy);
                continue;
            }
            time_t timestamp = atol(token);

            /* Skip entries outside the time range */
            if ((start_time > 0 && timestamp < start_time) ||
                (end_time > 0 && timestamp > end_time)) {
                free(line_copy);
                continue;
            }

            token = strtok(NULL, "|");
            if (!token) {
                free(line_copy);
                continue;
            }
            char* type = token;

            token = strtok(NULL, "|");
            if (!token) {
                free(line_copy);
                continue;
            }
            char* transaction_id = token;

            token = strtok(NULL, "\n");
            if (!token) {
                free(line_copy);
                continue;
            }
            char* data_json = token;

            /* Parse data JSON */
            json_value_t* data = json_parse(data_json);
            if (!data) {
                free(line_copy);
                continue;
            }

            /* We're only interested in STATE change entries */
            if (strcmp(type, "STATE") == 0) {
                json_value_t* state = json_object_get(data, "state");
                if (state && state->type == JSON_STRING) {
                    /* Find the correct time bucket */
                    int bucket_index = (timestamp - start_time) / 3600;
                    if (bucket_index < 0) bucket_index = 0;
                    if (bucket_index >= num_buckets) bucket_index = num_buckets - 1;

                    json_value_t* bucket = json_array_get(time_buckets, bucket_index);

                    /* Get transaction info from the map or create new entry */
                    json_value_t* tx_info = json_object_get(tx_map, transaction_id);
                    if (!tx_info) {
                        tx_info = json_create_object();
                        json_object_set(tx_info, "transaction_id", json_create_string(transaction_id));
                        json_object_set(tx_info, "start_time", json_create_integer(timestamp));
                        json_object_set(tx_map, transaction_id, tx_info);
                    }

                    /* Handle state changes */
                    if (strcmp(state->value.string, "active") == 0) {
                        /* Transaction started */
                        json_object_set(tx_info, "start_time", json_create_integer(timestamp));
                    } else if (strcmp(state->value.string, "committed") == 0 ||
                            strcmp(state->value.string, "aborted") == 0) {
                        /* Transaction completed */
                        json_value_t* start = json_object_get(tx_info, "start_time");
                        if (start && start->type == JSON_INTEGER) {
                            int64_t duration = timestamp - start->value.integer;

                            /* Update transaction in its bucket */
                            if (bucket) {
                                json_value_t* tx_count = json_object_get(bucket, "transactions");
                                if (tx_count && tx_count->type == JSON_INTEGER) {
                                    json_object_set(bucket, "transactions",
                                                json_create_integer(tx_count->value.integer + 1));
                                }

                                /* Update committed/aborted count */
                                const char* count_key = strcmp(state->value.string, "committed") == 0 ?
                                                    "committed" : "aborted";
                                json_value_t* state_count = json_object_get(bucket, count_key);
                                if (state_count && state_count->type == JSON_INTEGER) {
                                    json_object_set(bucket, count_key,
                                                json_create_integer(state_count->value.integer + 1));
                                }

                                /* Update average duration */
                                json_value_t* avg_duration = json_object_get(bucket, "avg_duration");
                                json_value_t* tx_count_updated = json_object_get(bucket, "transactions");
                                if (avg_duration && avg_duration->type == JSON_INTEGER &&
                                    tx_count_updated && tx_count_updated->type == JSON_INTEGER &&
                                    tx_count_updated->value.integer > 0) {

                                    int64_t new_avg = ((avg_duration->value.integer * (tx_count_updated->value.integer - 1)) +
                                                    duration) / tx_count_updated->value.integer;
                                    json_object_set(bucket, "avg_duration", json_create_integer(new_avg));
                                }
                            }

                            /* Update overall statistics if we're not using in-memory metrics */
                            if (!use_in_memory_metrics) {
                                total_transactions++;
                                if (strcmp(state->value.string, "committed") == 0) {
                                    committed_transactions++;
                                } else {
                                    aborted_transactions++;
                                }

                                total_duration += duration;
                                if (duration < min_duration) min_duration = duration;
                                if (duration > max_duration) max_duration = duration;
                            }
                        }

                        /* Remove from the map since we're done tracking it */
                        json_object_set(tx_map, transaction_id, NULL);
                    }
                }
            }

            json_free(data);
            free(line_copy);
        }

        /* Clean up */
        json_free(tx_map);
        fclose(file);
    }

    /* Update overall statistics */
    json_object_set(overall, "total_transactions", json_create_integer(total_transactions));
    json_object_set(overall, "committed_transactions", json_create_integer(committed_transactions));
    json_object_set(overall, "aborted_transactions", json_create_integer(aborted_transactions));
    json_object_set(overall, "active_transactions", json_create_integer(log->active_transactions));

    if (use_in_memory_metrics) {
        json_object_set(overall, "avg_duration", json_create_integer(log->avg_duration_ms / 1000)); /* Convert to seconds */
    } else if (total_transactions > 0) {
        json_object_set(overall, "avg_duration", json_create_integer(total_duration / total_transactions));
        json_object_set(overall, "min_duration", json_create_integer(min_duration == LONG_MAX ? 0 : min_duration));
        json_object_set(overall, "max_duration", json_create_integer(max_duration));
    }

    if (total_transactions > 0) {
        /* Create a string with the commit rate instead of using json_create_double which doesn't exist */
        char commit_rate_str[32];
        double commit_rate = (double)committed_transactions / total_transactions;
        snprintf(commit_rate_str, sizeof(commit_rate_str), "%.2f", commit_rate);
        json_object_set(overall, "commit_rate", json_create_string(commit_rate_str));
    }

    pthread_mutex_unlock(&log->lock);

    return metrics;
}