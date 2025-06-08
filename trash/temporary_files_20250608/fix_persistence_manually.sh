#!/bin/bash

# Manually fix the persistence.c file which has a broken function

cat > /tmp/persistence_fix.c << 'EOF'
/* Initialize persistence thread structure */
static persistence_thread_t* persistence_init(void) {
  persistence_thread_t* persistence = malloc(sizeof(persistence_thread_t));
  if (!persistence) {
    if (g_logger) LOG_ERROR("allocate persistence thread structure.");
    return NULL;
  }
  
  /* Initialize mutex and condition variable */
  if (pthread_mutex_init(&persistence->mutex, NULL) != 0) {
    if (g_logger) LOG_ERROR("initialize persistence mutex.");
    free(persistence);
    return NULL;
  }
  
  if (pthread_cond_init(&persistence->condition, NULL) != 0) {
    if (g_logger) LOG_ERROR("initialize persistence condition variable.");
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
  
  if (g_logger) TRACE_DB("Initialized.");
  return persistence;
}
EOF

# Replace the broken section in persistence.c
sed -i '11,44c\
/* Initialize persistence thread structure */\
static persistence_thread_t* persistence_init(void) {\
  persistence_thread_t* persistence = malloc(sizeof(persistence_thread_t));\
  if (!persistence) {\
    if (g_logger) LOG_ERROR("allocate persistence thread structure.");\
    return NULL;\
  }\
  \
  /* Initialize mutex and condition variable */\
  if (pthread_mutex_init(&persistence->mutex, NULL) != 0) {\
    if (g_logger) LOG_ERROR("initialize persistence mutex.");\
    free(persistence);\
    return NULL;\
  }\
  \
  if (pthread_cond_init(&persistence->condition, NULL) != 0) {\
    if (g_logger) LOG_ERROR("initialize persistence condition variable.");\
    pthread_mutex_destroy(&persistence->mutex);\
    free(persistence);\
    return NULL;\
  }\
  \
  /* Initialize fields */\
  persistence->shutdown = 0;\
  persistence->operations_count = 0;\
  persistence->data_size_estimate = 0;\
  persistence->last_save_time = time(NULL);\
  persistence->save_in_progress = 0;\
  persistence->last_save_failed = 0;\
  memset(persistence->last_error_message, 0, sizeof(persistence->last_error_message));\
  persistence->last_error_time = 0;\
  \
  if (g_logger) TRACE_DB("Initialized.");\
  return persistence;\
}' src/components/database/persistence.c

# Comment out the static function since it's not being used
sed -i 's/^static persistence_thread_t\* persistence_init/\/\/ static persistence_thread_t\* persistence_init/' src/components/database/persistence.c