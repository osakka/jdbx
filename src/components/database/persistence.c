#include "database/database.h"
#include "utils/logger.h"
#include <unistd.h>
#include <errno.h>

extern logger_config_t* g_logger;

/* Forward declaration */
static void* persistence_thread_main(void* arg);

/* Initialize persistence thread structure */
static persistence_thread_t* persistence_init(void) {
    persistence_thread_t* persistence = malloc(sizeof(persistence_thread_t));
    if (!persistence) {
        if (g_logger) LOG_ERROR("Failed to allocate persistence thread structure");
        return NULL;
    }
    
    /* Initialize mutex and condition variable */
    if (pthread_mutex_init(&persistence->mutex, NULL) != 0) {
        if (g_logger) LOG_ERROR("Failed to initialize persistence mutex");
        free(persistence);
        return NULL;
    }
    
    if (pthread_cond_init(&persistence->condition, NULL) != 0) {
        if (g_logger) LOG_ERROR("Failed to initialize persistence condition variable");
        pthread_mutex_destroy(&persistence->mutex);
        free(persistence);
        return NULL;
    }
    
    /* Initialize fields */
    persistence->shutdown = 0;
    persistence->operations_count = 0;
    persistence->data_size_estimate = 0;
    persistence->last_save_time = time(NULL);
    persistence->save_in_progress = 0;
    persistence->last_save_failed = 0;
    memset(persistence->last_error_message, 0, sizeof(persistence->last_error_message));
    persistence->last_error_time = 0;
    
    if (g_logger) LOG_INFO("Persistence thread structure initialized");
    return persistence;
}

/* Cleanup persistence thread structure */
static void persistence_cleanup(persistence_thread_t* persistence) {
    if (!persistence) return;
    
    pthread_mutex_destroy(&persistence->mutex);
    pthread_cond_destroy(&persistence->condition);
    free(persistence);
    
    if (g_logger) LOG_INFO("Persistence thread structure cleaned up");
}

/* Check if save is needed based on buffer thresholds */
static int should_save_now(persistence_thread_t* persistence, int force_periodic) {
    time_t current_time = time(NULL);
    
    /* Check buffer thresholds */
    if (persistence->operations_count >= PERSISTENCE_BUFFER_OPS_THRESHOLD) {
        if (g_logger) LOG_DEBUG("Save triggered by operations threshold: %d >= %d", 
                               persistence->operations_count, PERSISTENCE_BUFFER_OPS_THRESHOLD);
        return 1;
    }
    
    if (persistence->data_size_estimate >= PERSISTENCE_BUFFER_SIZE_THRESHOLD) {
        if (g_logger) LOG_DEBUG("Save triggered by data size threshold: %zu >= %d", 
                               persistence->data_size_estimate, PERSISTENCE_BUFFER_SIZE_THRESHOLD);
        return 1;
    }
    
    /* Check periodic save interval - only if there's data to save */
    if (force_periodic && (current_time - persistence->last_save_time) >= PERSISTENCE_PERIODIC_SAVE_INTERVAL) {
        /* Only trigger periodic save if there are operations or data changes */
        if (persistence->operations_count > 0 || persistence->data_size_estimate > 0) {
            if (g_logger) LOG_DEBUG("Save triggered by periodic interval: %ld seconds since last save", 
                                   current_time - persistence->last_save_time);
            return 1;
        } else {
            /* Update last_save_time to prevent continuous checking when no changes */
            persistence->last_save_time = current_time;
        }
    }
    
    return 0;
}

/* Reset buffer counters after save */
static void reset_buffer_counters(persistence_thread_t* persistence) {
    persistence->operations_count = 0;
    persistence->data_size_estimate = 0;
    persistence->last_save_time = time(NULL);
}

/* Main persistence thread function */
static void* persistence_thread_main(void* arg) {
    database_t* db = (database_t*)arg;
    persistence_thread_t* persistence = db->persistence;
    
    if (g_logger) LOG_INFO("Persistence thread started");
    
    pthread_mutex_lock(&persistence->mutex);
    
    while (!persistence->shutdown) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; /* Check every 1 second */
        
        /* Wait for notification or timeout */
        pthread_cond_timedwait(&persistence->condition, &persistence->mutex, &timeout);
        
        if (persistence->shutdown) break;
        
        /* Check if we need to save */
        int should_save_buffer = should_save_now(persistence, 0);
        int should_save_periodic = should_save_now(persistence, 1);
        
        /* TEMP: Debug persistence condition */
        if (should_save_buffer || should_save_periodic) {
            if (g_logger) LOG_DEBUG("PERSIST_DEBUG: should_save=true, db->is_modified=%d, save_in_progress=%d, ops=%d", 
                                   db->is_modified, persistence->save_in_progress, persistence->operations_count);
        }
        
        if ((should_save_buffer || should_save_periodic) && db->is_modified && !persistence->save_in_progress) {
            persistence->save_in_progress = 1;
            
            /* Unlock during save operation to avoid blocking other operations */
            pthread_mutex_unlock(&persistence->mutex);
            
            if (g_logger) LOG_INFO("Persistence thread triggering database save");
            
            /* Perform the save operation */
            int save_result = db_save(db);
            
            /* Lock again and update state */
            pthread_mutex_lock(&persistence->mutex);
            
            if (save_result) {
                if (g_logger) LOG_INFO("Persistence thread save completed successfully");
                reset_buffer_counters(persistence);
                persistence->last_save_failed = 0;
            } else {
                if (g_logger) LOG_ERROR("Persistence thread save failed");
                persistence->last_save_failed = 1;
                persistence->last_error_time = time(NULL);
                snprintf(persistence->last_error_message, sizeof(persistence->last_error_message),
                        "Database save failed during automatic persistence");
            }
            
            persistence->save_in_progress = 0;
            
            /* Signal any waiting threads that save is complete */
            pthread_cond_broadcast(&persistence->condition);
        }
    }
    
    pthread_mutex_unlock(&persistence->mutex);
    
    if (g_logger) LOG_INFO("Persistence thread exiting");
    return NULL;
}

/* Start persistence thread for a database */
int db_start_persistence_thread(database_t* db) {
    if (!db) {
        if (g_logger) LOG_ERROR("Cannot start persistence thread: NULL database");
        return 0;
    }
    
    if (db->persistence) {
        if (g_logger) LOG_WARNING("Persistence thread already exists for database");
        return 1; /* Already started */
    }
    
    /* Initialize persistence structure */
    db->persistence = persistence_init();
    if (!db->persistence) {
        return 0;
    }
    
    /* Create the persistence thread */
    if (pthread_create(&db->persistence->thread, NULL, persistence_thread_main, db) != 0) {
        if (g_logger) LOG_ERROR("Failed to create persistence thread: %s", strerror(errno));
        persistence_cleanup(db->persistence);
        db->persistence = NULL;
        return 0;
    }
    
    if (g_logger) LOG_INFO("Persistence thread started successfully for database: %s", db->path);
    return 1;
}

/* Stop persistence thread for a database */
void db_stop_persistence_thread(database_t* db) {
    if (!db || !db->persistence) {
        return;
    }
    
    persistence_thread_t* persistence = db->persistence;
    
    if (g_logger) LOG_INFO("Stopping persistence thread for database: %s", db->path);
    
    /* Signal shutdown */
    pthread_mutex_lock(&persistence->mutex);
    persistence->shutdown = 1;
    pthread_cond_signal(&persistence->condition);
    pthread_mutex_unlock(&persistence->mutex);
    
    /* Wait for thread to complete */
    pthread_join(persistence->thread, NULL);
    
    /* Cleanup */
    persistence_cleanup(persistence);
    db->persistence = NULL;
    
    if (g_logger) LOG_INFO("Persistence thread stopped successfully");
}

/* Notify persistence thread of data changes - returns 1 on success, 0 on error */
int db_notify_data_change_sync(database_t* db, size_t estimated_size) {
    if (!db || !db->persistence) {
        return 1; /* No persistence thread, assume success */
    }
    
    persistence_thread_t* persistence = db->persistence;
    
    pthread_mutex_lock(&persistence->mutex);
    
    /* Check for existing errors first */
    if (persistence->last_save_failed) {
        pthread_mutex_unlock(&persistence->mutex);
        return 0; /* Previous save failed */
    }
    
    /* Update buffer counters */
    persistence->operations_count++;
    persistence->data_size_estimate += estimated_size;
    
    if (g_logger) LOG_TRACE("Data change notification: ops=%d, size=%zu", 
                           persistence->operations_count, persistence->data_size_estimate);
    
    /* Check if immediate save is needed */
    if (should_save_now(persistence, 0)) {
        if (g_logger) LOG_DEBUG("PERSIST_DEBUG: Signaling persistence thread for immediate save");
        pthread_cond_signal(&persistence->condition);
        
        /* Wait for save to complete if needed */
        while (persistence->save_in_progress && !persistence->shutdown) {
            pthread_cond_wait(&persistence->condition, &persistence->mutex);
        }
        
        /* Check if save failed */
        if (persistence->last_save_failed) {
            pthread_mutex_unlock(&persistence->mutex);
            return 0; /* Save failed */
        }
    }
    
    pthread_mutex_unlock(&persistence->mutex);
    return 1; /* Success */
}

/* Async version for backward compatibility */
void db_notify_data_change(database_t* db, size_t estimated_size) {
    db_notify_data_change_sync(db, estimated_size);
}

/* Force immediate save operation */
int db_force_save(database_t* db) {
    if (!db) {
        return 0;
    }
    
    if (g_logger) LOG_INFO("Force save requested for database: %s", db->path);
    
    /* If persistence thread exists, notify it */
    if (db->persistence) {
        pthread_mutex_lock(&db->persistence->mutex);
        
        /* Force immediate save by maxing out counters */
        db->persistence->operations_count = PERSISTENCE_BUFFER_OPS_THRESHOLD;
        pthread_cond_signal(&db->persistence->condition);
        
        pthread_mutex_unlock(&db->persistence->mutex);
        
        /* Wait a moment for the save to complete */
        usleep(100000); /* 100ms */
    }
    
    /* Also perform direct save as fallback */
    return db_save(db);
}

/* Check for persistence errors */
int db_check_persistence_errors(database_t* db, char* error_buffer, size_t buffer_size) {
    if (!db || !db->persistence || !error_buffer || buffer_size == 0) {
        return 0; /* No errors or invalid parameters */
    }
    
    persistence_thread_t* persistence = db->persistence;
    
    pthread_mutex_lock(&persistence->mutex);
    
    int has_error = persistence->last_save_failed;
    if (has_error) {
        snprintf(error_buffer, buffer_size, "%s (occurred at %ld)", 
                persistence->last_error_message, persistence->last_error_time);
        
        /* Clear the error after reporting it */
        persistence->last_save_failed = 0;
        memset(persistence->last_error_message, 0, sizeof(persistence->last_error_message));
        persistence->last_error_time = 0;
    }
    
    pthread_mutex_unlock(&persistence->mutex);
    
    return has_error;
}