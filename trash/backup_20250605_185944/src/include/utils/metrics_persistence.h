#ifndef METRICS_PERSISTENCE_H
#define METRICS_PERSISTENCE_H

#include "utils/json.h"
#include <time.h>

/* Forward declaration */
struct database;

/**
 * Initialize metrics persistence system
 * @param db Database instance to store metrics
 * @return 1 on success, 0 on failure
 */
int metrics_persistence_init(struct database* db);

/**
 * Shutdown metrics persistence system
 */
void metrics_persistence_shutdown(void);

/**
 * Get historical metrics for a specific time range
 * @param start_time Start of time range (Unix timestamp)
 * @param end_time End of time range (Unix timestamp)
 * @param metric_name Optional specific metric name (NULL for all metrics)
 * @return JSON array of metric snapshots
 */
json_value_t* metrics_get_historical(time_t start_time, time_t end_time, const char* metric_name);

#endif /* METRICS_PERSISTENCE_H */