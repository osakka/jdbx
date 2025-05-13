#include "jsondb/utils/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

/* Get current timestamp in seconds */
double metrics_get_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1.0e9;
}

/* Get elapsed time from start in seconds */
double metrics_elapsed_time(struct timespec start) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec);
    elapsed += (end.tv_nsec - start.tv_nsec) / 1.0e9;
    
    return elapsed;
}

/* Create a new metrics registry */
metrics_registry_t* metrics_registry_create() {
    metrics_registry_t* registry = (metrics_registry_t*)malloc(sizeof(metrics_registry_t));
    if (!registry) {
        return NULL;
    }
    
    registry->metrics = NULL;
    pthread_mutex_init(&registry->mutex, NULL);
    registry->auto_export = 0;
    registry->export_path = NULL;
    registry->export_interval = 0;
    registry->export_thread_running = 0;
    
    return registry;
}

/* Free a metrics registry */
void metrics_registry_free(metrics_registry_t* registry) {
    if (!registry) {
        return;
    }
    
    /* Stop auto-export if running */
    if (registry->auto_export) {
        metrics_registry_disable_auto_export(registry);
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Free metrics */
    metric_t* current = registry->metrics;
    while (current) {
        metric_t* next = current->next;
        
        /* Free metric name and description */
        free(current->name);
        free(current->description);
        
        /* Free histogram buckets if needed */
        if (current->type == METRIC_TYPE_HISTOGRAM) {
            free(current->value.histogram.buckets);
            free(current->value.histogram.bucket_bounds);
        }
        
        /* Destroy mutex */
        pthread_mutex_destroy(&current->mutex);
        
        /* Free metric */
        free(current);
        
        current = next;
    }
    
    /* Free export path if needed */
    if (registry->export_path) {
        free(registry->export_path);
    }
    
    /* Unlock and destroy mutex */
    pthread_mutex_unlock(&registry->mutex);
    pthread_mutex_destroy(&registry->mutex);
    
    /* Free registry */
    free(registry);
}

/* Auto-export thread function */
static void* auto_export_thread(void* arg) {
    metrics_registry_t* registry = (metrics_registry_t*)arg;
    
    while (registry->export_thread_running) {
        /* Sleep for the specified interval */
        sleep(registry->export_interval);
        
        /* Check if we should still be running */
        if (!registry->export_thread_running) {
            break;
        }
        
        /* Export metrics */
        metrics_registry_export(registry, registry->export_path);
    }
    
    return NULL;
}

/* Enable auto-export of metrics */
void metrics_registry_enable_auto_export(metrics_registry_t* registry, const char* export_path, int interval) {
    if (!registry || !export_path || interval <= 0) {
        return;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Stop existing auto-export if running */
    if (registry->auto_export) {
        registry->export_thread_running = 0;
        pthread_join(registry->export_thread, NULL);
        
        if (registry->export_path) {
            free(registry->export_path);
            registry->export_path = NULL;
        }
    }
    
    /* Set auto-export parameters */
    registry->auto_export = 1;
    registry->export_path = strdup(export_path);
    registry->export_interval = interval;
    registry->export_thread_running = 1;
    
    /* Start auto-export thread */
    if (pthread_create(&registry->export_thread, NULL, auto_export_thread, registry) != 0) {
        /* Failed to create thread */
        registry->auto_export = 0;
        registry->export_thread_running = 0;
        
        if (registry->export_path) {
            free(registry->export_path);
            registry->export_path = NULL;
        }
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
}

/* Disable auto-export of metrics */
void metrics_registry_disable_auto_export(metrics_registry_t* registry) {
    if (!registry || !registry->auto_export) {
        return;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Stop auto-export thread */
    registry->export_thread_running = 0;
    
    /* Unlock registry while waiting for thread to exit */
    pthread_mutex_unlock(&registry->mutex);
    
    /* Wait for thread to exit */
    pthread_join(registry->export_thread, NULL);
    
    /* Lock registry again */
    pthread_mutex_lock(&registry->mutex);
    
    /* Clean up */
    registry->auto_export = 0;
    
    if (registry->export_path) {
        free(registry->export_path);
        registry->export_path = NULL;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
}

/* Generate JSON representation of metrics */
char* metrics_get_json(metrics_registry_t* registry) {
    if (!registry) {
        return NULL;
    }
    
    /* Initial buffer size */
    size_t buffer_size = 4096;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        return NULL;
    }
    
    size_t current_size = 0;
    
    /* Start JSON object */
    current_size += snprintf(buffer + current_size, buffer_size - current_size, "{\"metrics\":[");
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Export metrics */
    metric_t* current = registry->metrics;
    int first_metric = 1;
    
    while (current) {
        /* Check if we need to resize the buffer */
        if (buffer_size - current_size < 1024) {
            buffer_size *= 2;
            char* new_buffer = (char*)realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                pthread_mutex_unlock(&registry->mutex);
                return NULL;
            }
            buffer = new_buffer;
        }
        
        /* Add comma between metrics */
        if (!first_metric) {
            current_size += snprintf(buffer + current_size, buffer_size - current_size, ",");
        }
        first_metric = 0;
        
        /* Lock metric */
        pthread_mutex_lock(&current->mutex);
        
        /* Start metric object */
        current_size += snprintf(buffer + current_size, buffer_size - current_size, 
            "{\"name\":\"%s\",\"description\":\"%s\",\"type\":\"%s\",",
            current->name,
            current->description ? current->description : "",
            current->type == METRIC_TYPE_COUNTER ? "counter" :
            current->type == METRIC_TYPE_GAUGE ? "gauge" :
            current->type == METRIC_TYPE_TIMER ? "timer" : "histogram");
        
        /* Add values based on type */
        switch (current->type) {
            case METRIC_TYPE_COUNTER:
                current_size += snprintf(buffer + current_size, buffer_size - current_size,
                    "\"value\":%lld", (long long)current->value.counter);
                break;
            
            case METRIC_TYPE_GAUGE:
                current_size += snprintf(buffer + current_size, buffer_size - current_size,
                    "\"value\":%f", current->value.gauge);
                break;
            
            case METRIC_TYPE_TIMER:
                if (current->value.timer.count > 0) {
                    double mean = current->value.timer.sum / current->value.timer.count;
                    double variance = (current->value.timer.sum_squares / current->value.timer.count) - (mean * mean);
                    double stddev = sqrt(variance > 0 ? variance : 0);
                    
                    current_size += snprintf(buffer + current_size, buffer_size - current_size,
                        "\"count\":%llu,\"sum\":%f,\"min\":%f,\"max\":%f,\"mean\":%f,\"stddev\":%f",
                        (unsigned long long)current->value.timer.count,
                        current->value.timer.sum,
                        current->value.timer.min,
                        current->value.timer.max,
                        mean,
                        stddev);
                } else {
                    current_size += snprintf(buffer + current_size, buffer_size - current_size,
                        "\"count\":0,\"sum\":0,\"min\":0,\"max\":0,\"mean\":0,\"stddev\":0");
                }
                break;
            
            case METRIC_TYPE_HISTOGRAM:
                current_size += snprintf(buffer + current_size, buffer_size - current_size,
                    "\"count\":%llu,\"sum\":%f,\"min\":%f,\"max\":%f,\"buckets\":[",
                    (unsigned long long)current->value.histogram.count,
                    current->value.histogram.sum,
                    current->value.histogram.min,
                    current->value.histogram.max);
                
                for (size_t i = 0; i < current->value.histogram.num_buckets; i++) {
                    current_size += snprintf(buffer + current_size, buffer_size - current_size,
                        "%s{\"le\":%f,\"count\":%llu}",
                        i > 0 ? "," : "",
                        current->value.histogram.bucket_bounds[i],
                        (unsigned long long)current->value.histogram.buckets[i]);
                }
                
                current_size += snprintf(buffer + current_size, buffer_size - current_size, "]");
                break;
        }
        
        /* End metric object */
        current_size += snprintf(buffer + current_size, buffer_size - current_size, "}");
        
        /* Unlock metric */
        pthread_mutex_unlock(&current->mutex);
        
        current = current->next;
    }
    
    /* End JSON object */
    current_size += snprintf(buffer + current_size, buffer_size - current_size, "]}");
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    return buffer;
}

/* Export metrics to a file */
int metrics_registry_export(metrics_registry_t* registry, const char* export_path) {
    if (!registry || !export_path) {
        return 0;
    }
    
    /* Open file */
    FILE* file = fopen(export_path, "w");
    if (!file) {
        return 0;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Write metrics to file */
    metric_t* current = registry->metrics;
    
    /* Write timestamp and header */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(file, "# JSON Database Metrics - %s\n", timestamp);
    fprintf(file, "# Format: name type value [additional_fields]\n\n");
    
    /* Write metrics */
    while (current) {
        /* Lock metric */
        pthread_mutex_lock(&current->mutex);
        
        /* Write metric based on type */
        switch (current->type) {
            case METRIC_TYPE_COUNTER:
                fprintf(file, "%s counter %lld\n", 
                       current->name, (long long)current->value.counter);
                break;
                
            case METRIC_TYPE_GAUGE:
                fprintf(file, "%s gauge %.6f\n", 
                       current->name, current->value.gauge);
                break;
                
            case METRIC_TYPE_TIMER:
                fprintf(file, "%s timer count=%llu min=%.6f max=%.6f sum=%.6f avg=%.6f\n", 
                       current->name, 
                       (unsigned long long)current->value.timer.count, 
                       current->value.timer.min, 
                       current->value.timer.max, 
                       current->value.timer.sum,
                       current->value.timer.count > 0 ? 
                           current->value.timer.sum / current->value.timer.count : 0);
                break;
                
            case METRIC_TYPE_HISTOGRAM:
                fprintf(file, "%s histogram count=%llu min=%.6f max=%.6f sum=%.6f avg=%.6f\n", 
                       current->name, 
                       (unsigned long long)current->value.histogram.count, 
                       current->value.histogram.min, 
                       current->value.histogram.max, 
                       current->value.histogram.sum,
                       current->value.histogram.count > 0 ? 
                           current->value.histogram.sum / current->value.histogram.count : 0);
                
                /* Write histogram buckets */
                for (size_t i = 0; i < current->value.histogram.num_buckets; i++) {
                    fprintf(file, "%s bucket le=%.6f count=%llu\n", 
                           current->name, 
                           current->value.histogram.bucket_bounds[i], 
                           (unsigned long long)current->value.histogram.buckets[i]);
                }
                break;
        }
        
        /* Unlock metric */
        pthread_mutex_unlock(&current->mutex);
        
        current = current->next;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    /* Close file */
    fclose(file);
    
    return 1;
}

/* Find a metric by name */
static metric_t* find_metric(metrics_registry_t* registry, const char* name) {
    if (!registry || !name) {
        return NULL;
    }
    
    metric_t* current = registry->metrics;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/* Create a counter metric */
metric_t* metrics_create_counter(metrics_registry_t* registry, const char* name, const char* description) {
    if (!registry || !name) {
        return NULL;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Check if metric already exists */
    metric_t* existing = find_metric(registry, name);
    if (existing) {
        pthread_mutex_unlock(&registry->mutex);
        return existing->type == METRIC_TYPE_COUNTER ? existing : NULL;
    }
    
    /* Create new counter */
    metric_t* counter = (metric_t*)malloc(sizeof(metric_t));
    if (!counter) {
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Initialize counter */
    counter->name = strdup(name);
    counter->description = description ? strdup(description) : NULL;
    counter->type = METRIC_TYPE_COUNTER;
    counter->value.counter = 0;
    counter->next = NULL;
    
    pthread_mutex_init(&counter->mutex, NULL);
    
    /* Add to registry */
    if (!registry->metrics) {
        registry->metrics = counter;
    } else {
        metric_t* current = registry->metrics;
        while (current->next) {
            current = current->next;
        }
        current->next = counter;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    return counter;
}

/* Create a gauge metric */
metric_t* metrics_create_gauge(metrics_registry_t* registry, const char* name, const char* description) {
    if (!registry || !name) {
        return NULL;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Check if metric already exists */
    metric_t* existing = find_metric(registry, name);
    if (existing) {
        pthread_mutex_unlock(&registry->mutex);
        return existing->type == METRIC_TYPE_GAUGE ? existing : NULL;
    }
    
    /* Create new gauge */
    metric_t* gauge = (metric_t*)malloc(sizeof(metric_t));
    if (!gauge) {
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Initialize gauge */
    gauge->name = strdup(name);
    gauge->description = description ? strdup(description) : NULL;
    gauge->type = METRIC_TYPE_GAUGE;
    gauge->value.gauge = 0.0;
    gauge->next = NULL;
    
    pthread_mutex_init(&gauge->mutex, NULL);
    
    /* Add to registry */
    if (!registry->metrics) {
        registry->metrics = gauge;
    } else {
        metric_t* current = registry->metrics;
        while (current->next) {
            current = current->next;
        }
        current->next = gauge;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    return gauge;
}

/* Create a timer metric */
metric_t* metrics_create_timer(metrics_registry_t* registry, const char* name, const char* description) {
    if (!registry || !name) {
        return NULL;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Check if metric already exists */
    metric_t* existing = find_metric(registry, name);
    if (existing) {
        pthread_mutex_unlock(&registry->mutex);
        return existing->type == METRIC_TYPE_TIMER ? existing : NULL;
    }
    
    /* Create new timer */
    metric_t* timer = (metric_t*)malloc(sizeof(metric_t));
    if (!timer) {
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Initialize timer */
    timer->name = strdup(name);
    timer->description = description ? strdup(description) : NULL;
    timer->type = METRIC_TYPE_TIMER;
    timer->value.timer.count = 0;
    timer->value.timer.min = INFINITY;
    timer->value.timer.max = 0.0;
    timer->value.timer.sum = 0.0;
    timer->value.timer.sum_squares = 0.0;
    timer->next = NULL;
    
    pthread_mutex_init(&timer->mutex, NULL);
    
    /* Add to registry */
    if (!registry->metrics) {
        registry->metrics = timer;
    } else {
        metric_t* current = registry->metrics;
        while (current->next) {
            current = current->next;
        }
        current->next = timer;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    return timer;
}

/* Create a histogram metric */
metric_t* metrics_create_histogram(metrics_registry_t* registry, const char* name, const char* description, 
                                  double* bucket_bounds, size_t num_buckets) {
    if (!registry || !name || !bucket_bounds || num_buckets == 0) {
        return NULL;
    }
    
    /* Lock registry */
    pthread_mutex_lock(&registry->mutex);
    
    /* Check if metric already exists */
    metric_t* existing = find_metric(registry, name);
    if (existing) {
        pthread_mutex_unlock(&registry->mutex);
        return existing->type == METRIC_TYPE_HISTOGRAM ? existing : NULL;
    }
    
    /* Create new histogram */
    metric_t* histogram = (metric_t*)malloc(sizeof(metric_t));
    if (!histogram) {
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Initialize histogram */
    histogram->name = strdup(name);
    histogram->description = description ? strdup(description) : NULL;
    histogram->type = METRIC_TYPE_HISTOGRAM;
    histogram->value.histogram.count = 0;
    histogram->value.histogram.min = INFINITY;
    histogram->value.histogram.max = 0.0;
    histogram->value.histogram.sum = 0.0;
    histogram->value.histogram.num_buckets = num_buckets;
    
    /* Allocate buckets */
    histogram->value.histogram.buckets = (uint64_t*)calloc(num_buckets, sizeof(uint64_t));
    if (!histogram->value.histogram.buckets) {
        free(histogram->name);
        if (histogram->description) free(histogram->description);
        free(histogram);
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Allocate bucket bounds */
    histogram->value.histogram.bucket_bounds = (double*)malloc(num_buckets * sizeof(double));
    if (!histogram->value.histogram.bucket_bounds) {
        free(histogram->value.histogram.buckets);
        free(histogram->name);
        if (histogram->description) free(histogram->description);
        free(histogram);
        pthread_mutex_unlock(&registry->mutex);
        return NULL;
    }
    
    /* Copy bucket bounds */
    for (size_t i = 0; i < num_buckets; i++) {
        histogram->value.histogram.bucket_bounds[i] = bucket_bounds[i];
    }
    
    histogram->next = NULL;
    
    pthread_mutex_init(&histogram->mutex, NULL);
    
    /* Add to registry */
    if (!registry->metrics) {
        registry->metrics = histogram;
    } else {
        metric_t* current = registry->metrics;
        while (current->next) {
            current = current->next;
        }
        current->next = histogram;
    }
    
    /* Unlock registry */
    pthread_mutex_unlock(&registry->mutex);
    
    return histogram;
}

/* Increment a counter */
void metrics_counter_inc(metric_t* counter, int64_t value) {
    if (!counter || counter->type != METRIC_TYPE_COUNTER) {
        return;
    }
    
    pthread_mutex_lock(&counter->mutex);
    counter->value.counter += value;
    pthread_mutex_unlock(&counter->mutex);
}

/* Set a gauge value */
void metrics_gauge_set(metric_t* gauge, double value) {
    if (!gauge || gauge->type != METRIC_TYPE_GAUGE) {
        return;
    }
    
    pthread_mutex_lock(&gauge->mutex);
    gauge->value.gauge = value;
    pthread_mutex_unlock(&gauge->mutex);
}

/* Increment a gauge */
void metrics_gauge_inc(metric_t* gauge, double value) {
    if (!gauge || gauge->type != METRIC_TYPE_GAUGE) {
        return;
    }
    
    pthread_mutex_lock(&gauge->mutex);
    gauge->value.gauge += value;
    pthread_mutex_unlock(&gauge->mutex);
}

/* Decrement a gauge */
void metrics_gauge_dec(metric_t* gauge, double value) {
    if (!gauge || gauge->type != METRIC_TYPE_GAUGE) {
        return;
    }
    
    pthread_mutex_lock(&gauge->mutex);
    gauge->value.gauge -= value;
    pthread_mutex_unlock(&gauge->mutex);
}

/* Start a timer */
timer_context_t* metrics_timer_start(metric_t* timer) {
    if (!timer || timer->type != METRIC_TYPE_TIMER) {
        return NULL;
    }
    
    timer_context_t* context = (timer_context_t*)malloc(sizeof(timer_context_t));
    if (!context) {
        return NULL;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &context->start_time);
    context->metric = timer;
    
    return context;
}

/* Stop a timer */
void metrics_timer_stop(timer_context_t* context) {
    if (!context || !context->metric || context->metric->type != METRIC_TYPE_TIMER) {
        return;
    }
    
    /* Calculate elapsed time */
    double elapsed = metrics_elapsed_time(context->start_time);
    
    /* Update timer */
    metrics_timer_observe(context->metric, elapsed);
    
    /* Free context */
    free(context);
}

/* Observe a timer value directly */
void metrics_timer_observe(metric_t* timer, double seconds) {
    if (!timer || timer->type != METRIC_TYPE_TIMER || seconds < 0) {
        return;
    }
    
    pthread_mutex_lock(&timer->mutex);
    
    /* Update timer statistics */
    timer->value.timer.count++;
    timer->value.timer.sum += seconds;
    timer->value.timer.sum_squares += (seconds * seconds);
    
    if (seconds < timer->value.timer.min) {
        timer->value.timer.min = seconds;
    }
    
    if (seconds > timer->value.timer.max) {
        timer->value.timer.max = seconds;
    }
    
    pthread_mutex_unlock(&timer->mutex);
}

/* Observe a histogram value */
void metrics_histogram_observe(metric_t* histogram, double value) {
    if (!histogram || histogram->type != METRIC_TYPE_HISTOGRAM) {
        return;
    }
    
    pthread_mutex_lock(&histogram->mutex);
    
    /* Update histogram statistics */
    histogram->value.histogram.count++;
    histogram->value.histogram.sum += value;
    
    if (value < histogram->value.histogram.min) {
        histogram->value.histogram.min = value;
    }
    
    if (value > histogram->value.histogram.max) {
        histogram->value.histogram.max = value;
    }
    
    /* Update buckets */
    for (size_t i = 0; i < histogram->value.histogram.num_buckets; i++) {
        if (value <= histogram->value.histogram.bucket_bounds[i]) {
            histogram->value.histogram.buckets[i]++;
        }
    }
    
    pthread_mutex_unlock(&histogram->mutex);
}