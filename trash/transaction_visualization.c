#include "transaction/transaction.h"
#include "api/api.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Extract transaction history data for visualization
 * Note: This has been moved to transaction_visualization_enhanced.c
 * This is a stub to avoid breaking existing code
 */
static json_value_t* __attribute__((unused)) transaction_visualization_get_history(transaction_manager_t* manager, time_t start_time, time_t end_time,
                    const char* user_id, int limit) {
  if (!manager || !manager->log) {
    return NULL;
  }
  
  json_value_t* history = json_create_array();
  if (!history) {
    return NULL;
  }
  
  /* Use the transaction log to gather history data */
  transaction_log_t* log = manager->log;
  FILE* file = fopen(log->log_file, "r");
  if (!file) {
    json_free(history);
    return NULL;
  }
  
  pthread_mutex_lock(&log->lock);
  
  char line[4096];
  int count = 0;
  int max_count = limit > 0 ? limit : 1000; /* Default limit to 1000 if not specified */
  
  /* Process the log file */
  while (fgets(line, sizeof(line), file) && count < max_count) {
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
    /* Need transaction_id for data structure */
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
    
    /* Filter by user_id if specified */
    if (user_id && *user_id) {
      json_value_t* data_user_id = json_object_get(data, "user_id");
      if (!data_user_id || data_user_id->type != JSON_STRING || 
        strcmp(data_user_id->value.string, user_id) != 0) {
        json_free(data);
        free(line_copy);
        continue;
      }
    }
    
    /* Create a history entry with relevant data */
    json_value_t* entry = json_create_object();
    if (!entry) {
      json_free(data);
      free(line_copy);
      continue;
    }
    
    /* Add common fields */
    json_object_set(entry, "timestamp", json_create_integer(timestamp));
    json_object_set(entry, "type", json_create_string(type));
    json_object_set(entry, "transaction_id", json_create_string(transaction_id));
    
    /* Add data based on entry type */
    if (strcmp(type, "STATE") == 0) {
      json_value_t* state = json_object_get(data, "state");
      if (state && state->type == JSON_STRING) {
        json_object_set(entry, "state", json_create_string(state->value.string));
      }
      
      json_value_t* user = json_object_get(data, "user_id");
      if (user && user->type == JSON_STRING) {
        json_object_set(entry, "user_id", json_create_string(user->value.string));
      }
      
      json_value_t* isolation = json_object_get(data, "isolation_level");
      if (isolation && isolation->type == JSON_STRING) {
        json_object_set(entry, "isolation_level", json_create_string(isolation->value.string));
      }
      
      json_value_t* commit_time = json_object_get(data, "commit_time");
      if (commit_time && commit_time->type == JSON_INTEGER) {
        json_object_set(entry, "commit_time", json_create_integer(commit_time->value.integer));
        
        /* Calculate transaction duration if it was committed */
        if (timestamp < commit_time->value.integer) {
          json_object_set(entry, "duration", 
                 json_create_integer(commit_time->value.integer - timestamp));
        }
      }
    } else if (strcmp(type, "OPERATION") == 0) {
      json_value_t* op_type = json_object_get(data, "type");
      if (op_type && op_type->type == JSON_STRING) {
        json_object_set(entry, "operation", json_create_string(op_type->value.string));
      }
      
      json_value_t* collection = json_object_get(data, "collection");
      if (collection && collection->type == JSON_STRING) {
        json_object_set(entry, "collection", json_create_string(collection->value.string));
      }
      
      json_value_t* document_id = json_object_get(data, "document_id");
      if (document_id && document_id->type == JSON_STRING) {
        json_object_set(entry, "document_id", json_create_string(document_id->value.string));
      }
      
      /* Count operation complexity (approximate) */
      int complexity = 1;
      
      json_value_t* before_state = json_object_get(data, "before_state");
      if (before_state && before_state->type == JSON_OBJECT) {
        complexity += json_object_size(before_state) / 2;
      }
      
      json_value_t* after_state = json_object_get(data, "after_state");
      if (after_state && after_state->type == JSON_OBJECT) {
        complexity += json_object_size(after_state) / 2;
      }
      
      json_object_set(entry, "complexity", json_create_integer(complexity));
    } else if (strcmp(type, "AUDIT") == 0) {
      /* For audit entries, include relevant metrics */
      json_value_t* duration = json_object_get(data, "duration_ms");
      if (duration && duration->type == JSON_INTEGER) {
        json_object_set(entry, "duration_ms", json_create_integer(duration->value.integer));
      }
      
      json_value_t* op_count = json_object_get(data, "operation_count");
      if (op_count && op_count->type == JSON_INTEGER) {
        json_object_set(entry, "operation_count", json_create_integer(op_count->value.integer));
      }
      
      json_value_t* error_code = json_object_get(data, "error_code");
      if (error_code && error_code->type == JSON_INTEGER) {
        json_object_set(entry, "error_code", json_create_integer(error_code->value.integer));
      }
      
      json_value_t* client_ip = json_object_get(data, "client_ip");
      if (client_ip && client_ip->type == JSON_STRING) {
        json_object_set(entry, "client_ip", json_create_string(client_ip->value.string));
      }
    }
    
    /* Add the entry to the history array */
    json_array_append(history, entry);
    count++;
    
    /* Clean up */
    json_free(data);
    free(line_copy);
  }
  
  fclose(file);
  pthread_mutex_unlock(&log->lock);
  
  return history;
}

/* Get transaction performance metrics by different dimensions
 * Note: This has been moved to transaction.c
 * This is a stub to avoid breaking existing code
 */
static json_value_t* __attribute__((unused)) transaction_visualization_get_performance_metrics(transaction_manager_t* manager,
                        const char* dimension, time_t start_time, time_t end_time) {
  if (!manager || !manager->log || !dimension) {
    return NULL;
  }
  
  /* Valid dimensions: 
   * - "time" (metrics by hour or day)
   * - "user" (metrics by user)
   * - "isolation" (metrics by isolation level)
   * - "collection" (metrics by collection)
   */
  
  json_value_t* metrics = json_create_object();
  if (!metrics) {
    return NULL;
  }
  
  /* Create result structure based on dimension */
  json_value_t* metrics_data = NULL;
  
  if (strcmp(dimension, "time") == 0) {
    metrics_data = json_create_object();
    if (!metrics_data) {
      json_free(metrics);
      return NULL;
    }
    
    json_object_set(metrics, "dimension", json_create_string("time"));
    json_object_set(metrics, "unit", json_create_string("hour"));
    json_object_set(metrics, "data", metrics_data);
  } 
  else if (strcmp(dimension, "user") == 0) {
    metrics_data = json_create_object();
    if (!metrics_data) {
      json_free(metrics);
      return NULL;
    }
    
    json_object_set(metrics, "dimension", json_create_string("user"));
    json_object_set(metrics, "data", metrics_data);
  }
  else if (strcmp(dimension, "isolation") == 0) {
    metrics_data = json_create_object();
    if (!metrics_data) {
      json_free(metrics);
      return NULL;
    }
    
    json_object_set(metrics, "dimension", json_create_string("isolation"));
    json_object_set(metrics, "data", metrics_data);
  }
  else if (strcmp(dimension, "collection") == 0) {
    metrics_data = json_create_object();
    if (!metrics_data) {
      json_free(metrics);
      return NULL;
    }
    
    json_object_set(metrics, "dimension", json_create_string("collection"));
    json_object_set(metrics, "data", metrics_data);
  }
  else {
    /* Invalid dimension */
    json_free(metrics);
    return NULL;
  }
  
  /* Analyze transaction log to gather metrics */
  transaction_log_t* log = manager->log;
  FILE* file = fopen(log->log_file, "r");
  if (!file) {
    json_free(metrics);
    return NULL;
  }
  
  pthread_mutex_lock(&log->lock);
  
  char line[4096];
  
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
    /* Need transaction_id for data structure */
    /* Skip transaction_id token, not needed in this context */
    
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
    
    /* Process based on dimension */
    if (strcmp(dimension, "time") == 0) {
      /* Group by hour */
      struct tm* time_info = localtime(&timestamp);
      char hour_key[32]; /* Increased buffer size */
      snprintf(hour_key, sizeof(hour_key), "%04d-%02d-%02d-%02d", 
          time_info->tm_year + 1900, 
          time_info->tm_mon + 1, 
          time_info->tm_mday,
          time_info->tm_hour);
      
      /* Get or create hourly stats */
      json_value_t* hour_stats = json_object_get(metrics_data, hour_key);
      if (!hour_stats) {
        hour_stats = json_create_object();
        if (!hour_stats) {
          json_free(data);
          free(line_copy);
          continue;
        }
        
        json_object_set(hour_stats, "transaction_count", json_create_integer(0));
        json_object_set(hour_stats, "operation_count", json_create_integer(0));
        json_object_set(hour_stats, "commit_count", json_create_integer(0));
        json_object_set(hour_stats, "abort_count", json_create_integer(0));
        json_object_set(hour_stats, "avg_duration_ms", json_create_integer(0));
        json_object_set(hour_stats, "total_duration_ms", json_create_integer(0));
        
        json_object_set(metrics_data, hour_key, hour_stats);
      }
      
      /* Increment counts based on entry type */
      if (strcmp(type, "STATE") == 0) {
        json_value_t* state = json_object_get(data, "state");
        if (state && state->type == JSON_STRING) {
          if (strcmp(state->value.string, "committed") == 0) {
            /* Increment commit count */
            json_value_t* commit_count = json_object_get(hour_stats, "commit_count");
            if (commit_count && commit_count->type == JSON_INTEGER) {
              json_object_set(hour_stats, "commit_count", 
                     json_create_integer(commit_count->value.integer + 1));
            }
            
            /* If there's a duration value, update average duration */
            json_value_t* commit_time = json_object_get(data, "commit_time");
            if (commit_time && commit_time->type == JSON_INTEGER) {
              int duration_ms = (commit_time->value.integer - timestamp) * 1000;
              
              json_value_t* total_duration = json_object_get(hour_stats, "total_duration_ms");
              json_value_t* transaction_count = json_object_get(hour_stats, "transaction_count");
              
              if (total_duration && total_duration->type == JSON_INTEGER &&
                transaction_count && transaction_count->type == JSON_INTEGER) {
                /* Update total duration */
                int new_total = total_duration->value.integer + duration_ms;
                json_object_set(hour_stats, "total_duration_ms", json_create_integer(new_total));
                
                /* Update average */
                int new_avg = new_total / (transaction_count->value.integer + 1);
                json_object_set(hour_stats, "avg_duration_ms", json_create_integer(new_avg));
              }
            }
          } 
          else if (strcmp(state->value.string, "aborted") == 0) {
            /* Increment abort count */
            json_value_t* abort_count = json_object_get(hour_stats, "abort_count");
            if (abort_count && abort_count->type == JSON_INTEGER) {
              json_object_set(hour_stats, "abort_count", 
                     json_create_integer(abort_count->value.integer + 1));
            }
          }
          else if (strcmp(state->value.string, "active") == 0) {
            /* Increment transaction count */
            json_value_t* transaction_count = json_object_get(hour_stats, "transaction_count");
            if (transaction_count && transaction_count->type == JSON_INTEGER) {
              json_object_set(hour_stats, "transaction_count", 
                     json_create_integer(transaction_count->value.integer + 1));
            }
          }
        }
      }
      else if (strcmp(type, "OPERATION") == 0) {
        /* Increment operation count */
        json_value_t* operation_count = json_object_get(hour_stats, "operation_count");
        if (operation_count && operation_count->type == JSON_INTEGER) {
          json_object_set(hour_stats, "operation_count", 
                 json_create_integer(operation_count->value.integer + 1));
        }
      }
    }
    else if (strcmp(dimension, "user") == 0) {
      /* Extract user_id from the data */
      json_value_t* user_id = json_object_get(data, "user_id");
      if (user_id && user_id->type == JSON_STRING) {
        const char* user_key = user_id->value.string;
        
        /* Get or create user stats */
        json_value_t* user_stats = json_object_get(metrics_data, user_key);
        if (!user_stats) {
          user_stats = json_create_object();
          if (!user_stats) {
            json_free(data);
            free(line_copy);
            continue;
          }
          
          json_object_set(user_stats, "transaction_count", json_create_integer(0));
          json_object_set(user_stats, "operation_count", json_create_integer(0));
          json_object_set(user_stats, "commit_count", json_create_integer(0));
          json_object_set(user_stats, "abort_count", json_create_integer(0));
          json_object_set(user_stats, "avg_duration_ms", json_create_integer(0));
          json_object_set(user_stats, "total_duration_ms", json_create_integer(0));
          
          json_object_set(metrics_data, user_key, user_stats);
        }
        
        /* Update metrics similar to time dimension */
        if (strcmp(type, "STATE") == 0) {
          json_value_t* state = json_object_get(data, "state");
          if (state && state->type == JSON_STRING) {
            if (strcmp(state->value.string, "committed") == 0) {
              /* Update commit count and duration metrics */
              json_value_t* commit_count = json_object_get(user_stats, "commit_count");
              if (commit_count && commit_count->type == JSON_INTEGER) {
                json_object_set(user_stats, "commit_count", 
                       json_create_integer(commit_count->value.integer + 1));
              }
              
              /* Calculate duration if applicable */
              /* Code similar to time dimension... */
            } 
            else if (strcmp(state->value.string, "aborted") == 0) {
              /* Update abort count */
              json_value_t* abort_count = json_object_get(user_stats, "abort_count");
              if (abort_count && abort_count->type == JSON_INTEGER) {
                json_object_set(user_stats, "abort_count", 
                       json_create_integer(abort_count->value.integer + 1));
              }
            }
            else if (strcmp(state->value.string, "active") == 0) {
              /* Update transaction count */
              json_value_t* transaction_count = json_object_get(user_stats, "transaction_count");
              if (transaction_count && transaction_count->type == JSON_INTEGER) {
                json_object_set(user_stats, "transaction_count", 
                       json_create_integer(transaction_count->value.integer + 1));
              }
            }
          }
        }
        else if (strcmp(type, "OPERATION") == 0) {
          /* Update operation count */
          json_value_t* operation_count = json_object_get(user_stats, "operation_count");
          if (operation_count && operation_count->type == JSON_INTEGER) {
            json_object_set(user_stats, "operation_count", 
                   json_create_integer(operation_count->value.integer + 1));
          }
        }
      }
    }
    else if (strcmp(dimension, "isolation") == 0) {
      /* Only process STATE entries with isolation level */
      if (strcmp(type, "STATE") == 0 && strcmp(type, "STATE") == 0) {
        json_value_t* isolation_level = json_object_get(data, "isolation_level");
        if (isolation_level && isolation_level->type == JSON_STRING) {
          const char* isolation_key = isolation_level->value.string;
          
          /* Get or create isolation stats */
          json_value_t* isolation_stats = json_object_get(metrics_data, isolation_key);
          if (!isolation_stats) {
            isolation_stats = json_create_object();
            if (!isolation_stats) {
              json_free(data);
              free(line_copy);
              continue;
            }
            
            json_object_set(isolation_stats, "transaction_count", json_create_integer(0));
            json_object_set(isolation_stats, "commit_count", json_create_integer(0));
            json_object_set(isolation_stats, "abort_count", json_create_integer(0));
            json_object_set(isolation_stats, "avg_duration_ms", json_create_integer(0));
            
            json_object_set(metrics_data, isolation_key, isolation_stats);
          }
          
          /* Update metrics based on state */
          json_value_t* state = json_object_get(data, "state");
          if (state && state->type == JSON_STRING) {
            if (strcmp(state->value.string, "active") == 0) {
              /* Increment transaction count */
              json_value_t* transaction_count = json_object_get(isolation_stats, "transaction_count");
              if (transaction_count && transaction_count->type == JSON_INTEGER) {
                json_object_set(isolation_stats, "transaction_count", 
                       json_create_integer(transaction_count->value.integer + 1));
              }
            }
            else if (strcmp(state->value.string, "committed") == 0) {
              /* Increment commit count */
              json_value_t* commit_count = json_object_get(isolation_stats, "commit_count");
              if (commit_count && commit_count->type == JSON_INTEGER) {
                json_object_set(isolation_stats, "commit_count", 
                       json_create_integer(commit_count->value.integer + 1));
              }
            }
            else if (strcmp(state->value.string, "aborted") == 0) {
              /* Increment abort count */
              json_value_t* abort_count = json_object_get(isolation_stats, "abort_count");
              if (abort_count && abort_count->type == JSON_INTEGER) {
                json_object_set(isolation_stats, "abort_count", 
                       json_create_integer(abort_count->value.integer + 1));
              }
            }
          }
        }
      }
    }
    else if (strcmp(dimension, "collection") == 0) {
      /* Only process OPERATION entries with collection */
      if (strcmp(type, "OPERATION") == 0) {
        json_value_t* collection = json_object_get(data, "collection");
        if (collection && collection->type == JSON_STRING) {
          const char* collection_key = collection->value.string;
          
          /* Get or create collection stats */
          json_value_t* collection_stats = json_object_get(metrics_data, collection_key);
          if (!collection_stats) {
            collection_stats = json_create_object();
            if (!collection_stats) {
              json_free(data);
              free(line_copy);
              continue;
            }
            
            json_object_set(collection_stats, "operation_count", json_create_integer(0));
            json_object_set(collection_stats, "insert_count", json_create_integer(0));
            json_object_set(collection_stats, "update_count", json_create_integer(0));
            json_object_set(collection_stats, "delete_count", json_create_integer(0));
            
            json_object_set(metrics_data, collection_key, collection_stats);
          }
          
          /* Increment operation count */
          json_value_t* operation_count = json_object_get(collection_stats, "operation_count");
          if (operation_count && operation_count->type == JSON_INTEGER) {
            json_object_set(collection_stats, "operation_count", 
                   json_create_integer(operation_count->value.integer + 1));
          }
          
          /* Increment specific operation type count */
          json_value_t* op_type = json_object_get(data, "type");
          if (op_type && op_type->type == JSON_STRING) {
            if (strcmp(op_type->value.string, "insert") == 0) {
              json_value_t* insert_count = json_object_get(collection_stats, "insert_count");
              if (insert_count && insert_count->type == JSON_INTEGER) {
                json_object_set(collection_stats, "insert_count", 
                       json_create_integer(insert_count->value.integer + 1));
              }
            }
            else if (strcmp(op_type->value.string, "update") == 0) {
              json_value_t* update_count = json_object_get(collection_stats, "update_count");
              if (update_count && update_count->type == JSON_INTEGER) {
                json_object_set(collection_stats, "update_count", 
                       json_create_integer(update_count->value.integer + 1));
              }
            }
            else if (strcmp(op_type->value.string, "delete") == 0) {
              json_value_t* delete_count = json_object_get(collection_stats, "delete_count");
              if (delete_count && delete_count->type == JSON_INTEGER) {
                json_object_set(collection_stats, "delete_count", 
                       json_create_integer(delete_count->value.integer + 1));
              }
            }
          }
        }
      }
    }
    
    /* Clean up */
    json_free(data);
    free(line_copy);
  }
  
  fclose(file);
  pthread_mutex_unlock(&log->lock);
  
  return metrics;
}

/* API handler for transaction history visualization */
http_response_t* api_handle_visualization_transaction_history(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract database from context */
  database_t* db = ctx->db;
  if (!db || !db->transaction_manager || !db->transaction_manager->log) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Transaction log not available\"}", "application/json");
  }
  
  /* Parse query parameters */
  char* query_string = request->query;
  time_t start_time = 0;
  time_t end_time = time(NULL); /* Default to current time */
  char user_id[256] = {0};
  int limit = 100; /* Default limit */
  
  if (query_string) {
    /* Parse start_time */
    char* start_param = strstr(query_string, "start_time=");
    if (start_param) {
      start_time = (time_t)atol(start_param + 11);
    }
    
    /* Parse end_time */
    char* end_param = strstr(query_string, "end_time=");
    if (end_param) {
      end_time = (time_t)atol(end_param + 9);
    }
    
    /* Parse user_id */
    char* user_param = strstr(query_string, "user_id=");
    if (user_param) {
      char* user_value = user_param + 8;
      char* user_end = strchr(user_value, '&');
      size_t user_len = user_end ? (size_t)(user_end - user_value) : strlen(user_value);
      if (user_len >= sizeof(user_id)) {
        user_len = sizeof(user_id) - 1;
      }
      strncpy(user_id, user_value, user_len);
      user_id[user_len] = '\0';
    }
    
    /* Parse limit */
    char* limit_param = strstr(query_string, "limit=");
    if (limit_param) {
      limit = atoi(limit_param + 6);
      if (limit <= 0) {
        limit = 100; /* Reset to default if invalid */
      }
    }
  }
  
  /* Get transaction history data */
  transaction_manager_t* manager = db->transaction_manager;
  json_value_t* history = transaction_get_history_data(manager, start_time, end_time, 
                           user_id[0] ? user_id : NULL, limit);
  
  if (!history) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to retrieve transaction history\"}", "application/json");
  }
  
  /* Create the response object */
  json_value_t* response_obj = json_create_object();
  if (!response_obj) {
    json_free(history);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create response\"}", "application/json");
  }
  
  /* Add metadata */
  json_object_set(response_obj, "start_time", json_create_integer(start_time));
  json_object_set(response_obj, "end_time", json_create_integer(end_time));
  json_object_set(response_obj, "limit", json_create_integer(limit));
  if (user_id[0]) {
    json_object_set(response_obj, "user_id", json_create_string(user_id));
  }
  json_object_set(response_obj, "count", json_create_integer(json_array_size(history)));
  
  /* Add history data */
  json_object_set(response_obj, "history", history);
  
  /* Convert to response */
  char* json_str = json_stringify(response_obj);
  json_free(response_obj);
  
  if (!json_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize transaction history\"}", "application/json");
  }
  
  http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
  free(json_str);
  
  return response;
}

/* API handler for transaction performance metrics visualization */
http_response_t* api_handle_visualization_transaction_metrics(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract database from context */
  database_t* db = ctx->db;
  if (!db || !db->transaction_manager || !db->transaction_manager->log) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Transaction log not available\"}", "application/json");
  }
  
  /* Parse query parameters */
  char* query_string = request->query;
  time_t start_time = 0;
  time_t end_time = time(NULL); /* Default to current time */
  char dimension[32] = "time"; /* Default dimension */
  
  if (query_string) {
    /* Parse start_time */
    char* start_param = strstr(query_string, "start_time=");
    if (start_param) {
      start_time = (time_t)atol(start_param + 11);
    }
    
    /* Parse end_time */
    char* end_param = strstr(query_string, "end_time=");
    if (end_param) {
      end_time = (time_t)atol(end_param + 9);
    }
    
    /* Parse dimension */
    char* dim_param = strstr(query_string, "dimension=");
    if (dim_param) {
      char* dim_value = dim_param + 10;
      char* dim_end = strchr(dim_value, '&');
      size_t dim_len = dim_end ? (size_t)(dim_end - dim_value) : strlen(dim_value);
      if (dim_len >= sizeof(dimension)) {
        dim_len = sizeof(dimension) - 1;
      }
      strncpy(dimension, dim_value, dim_len);
      dimension[dim_len] = '\0';
    }
  }
  
  /* Get transaction metrics data */
  transaction_manager_t* manager = db->transaction_manager;
  /* Call the actual function from transaction.c */
  json_value_t* metrics = transaction_get_performance_metrics(manager, dimension, start_time, end_time);
  
  if (!metrics) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to retrieve transaction metrics\"}", "application/json");
  }
  
  /* Add metadata */
  json_object_set(metrics, "start_time", json_create_integer(start_time));
  json_object_set(metrics, "end_time", json_create_integer(end_time));
  
  /* Convert to response */
  char* json_str = json_stringify(metrics);
  json_free(metrics);
  
  if (!json_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize transaction metrics\"}", "application/json");
  }
  
  http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
  free(json_str);
  
  return response;
}

/* Suppress unused function warnings by referencing them */
static void __attribute__((unused)) suppress_unused_function_warnings(void) {
  if (0) {
    transaction_visualization_get_history(NULL, 0, 0, NULL, 0);
    transaction_visualization_get_performance_metrics(NULL, NULL, 0, 0);
  }
}

/* API handler for transaction relationship visualization */
http_response_t* api_handle_visualization_transaction_relationships(api_context_t* ctx, http_request_t* request) {
  if (!ctx || !request) {
    return create_http_response(HTTP_BAD_REQUEST, 
                 "{\"error\":\"Invalid request\"}", "application/json");
  }
  
  /* Extract database from context */
  database_t* db = ctx->db;
  if (!db || !db->transaction_manager || !db->transaction_manager->log) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Transaction log not available\"}", "application/json");
  }
  
  /* Parse query parameters */
  char* query_string = request->query;
  time_t start_time = 0;
  time_t end_time = time(NULL); /* Default to current time */
  char collection[256] = {0};
  char document_id[256] = {0};
  
  if (query_string) {
    /* Parse start_time */
    char* start_param = strstr(query_string, "start_time=");
    if (start_param) {
      start_time = (time_t)atol(start_param + 11);
    }
    
    /* Parse end_time */
    char* end_param = strstr(query_string, "end_time=");
    if (end_param) {
      end_time = (time_t)atol(end_param + 9);
    }
    
    /* Parse collection */
    char* collection_param = strstr(query_string, "collection=");
    if (collection_param) {
      char* coll_value = collection_param + 11;
      char* coll_end = strchr(coll_value, '&');
      size_t coll_len = coll_end ? (size_t)(coll_end - coll_value) : strlen(coll_value);
      if (coll_len >= sizeof(collection)) {
        coll_len = sizeof(collection) - 1;
      }
      strncpy(collection, coll_value, coll_len);
      collection[coll_len] = '\0';
    }
    
    /* Parse document_id */
    char* doc_param = strstr(query_string, "document_id=");
    if (doc_param) {
      char* doc_value = doc_param + 12;
      char* doc_end = strchr(doc_value, '&');
      size_t doc_len = doc_end ? (size_t)(doc_end - doc_value) : strlen(doc_value);
      if (doc_len >= sizeof(document_id)) {
        doc_len = sizeof(document_id) - 1;
      }
      strncpy(document_id, doc_value, doc_len);
      document_id[doc_len] = '\0';
    }
  }
  
  /* Create response structure */
  json_value_t* relationship_data = json_create_object();
  if (!relationship_data) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create relationship data\"}", "application/json");
  }
  
  json_object_set(relationship_data, "start_time", json_create_integer(start_time));
  json_object_set(relationship_data, "end_time", json_create_integer(end_time));
  
  /* Nodes and edges for graph visualization */
  json_value_t* nodes = json_create_array();
  json_value_t* edges = json_create_array();
  
  if (!nodes || !edges) {
    json_free(relationship_data);
    if (nodes) json_free(nodes);
    if (edges) json_free(edges);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to create graph data structures\"}", "application/json");
  }
  
  /* Create graph data from transaction log */
  transaction_log_t* log = db->transaction_manager->log;
  FILE* file = fopen(log->log_file, "r");
  if (!file) {
    json_free(relationship_data);
    json_free(nodes);
    json_free(edges);
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to open transaction log file\"}", "application/json");
  }
  
  pthread_mutex_lock(&log->lock);
  
  /* Track transactions and documents for creating graph */
  #define MAX_GRAPH_TRANSACTIONS 1000
  #define MAX_GRAPH_DOCUMENTS 1000
  
  /* Transaction nodes */
  char* transaction_ids[MAX_GRAPH_TRANSACTIONS] = {NULL};
  int transaction_count = 0;
  
  /* Document nodes */
  char* document_collections[MAX_GRAPH_DOCUMENTS] = {NULL};
  char* document_ids[MAX_GRAPH_DOCUMENTS] = {NULL};
  int document_count = 0;
  
  char line[4096];
  
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
    /* Need transaction_id for data structure */
    char* transaction_id = token;
    
    token = strtok(NULL, "\n");
    if (!token) {
      free(line_copy);
      continue;
    }
    char* data_json = token;
    
    /* Add transaction node if not already added */
    int transaction_index = -1;
    for (int i = 0; i < transaction_count; i++) {
      if (transaction_ids[i] && strcmp(transaction_ids[i], transaction_id) == 0) {
        transaction_index = i;
        break;
      }
    }
    
    if (transaction_index == -1 && transaction_count < MAX_GRAPH_TRANSACTIONS) {
      /* Add new transaction node */
      transaction_ids[transaction_count] = strdup(transaction_id);
      if (!transaction_ids[transaction_count]) {
        free(line_copy);
        continue;
      }
      
      transaction_index = transaction_count++;
      
      /* Create node */
      json_value_t* node = json_create_object();
      if (node) {
        json_object_set(node, "id", json_create_string(transaction_id));
        json_object_set(node, "type", json_create_string("transaction"));
        json_object_set(node, "timestamp", json_create_integer(timestamp));
        
        /* Parse data JSON */
        json_value_t* data = json_parse(data_json);
        if (data) {
          if (strcmp(type, "STATE") == 0) {
            json_value_t* state = json_object_get(data, "state");
            if (state && state->type == JSON_STRING) {
              json_object_set(node, "state", json_create_string(state->value.string));
            }
            
            json_value_t* user = json_object_get(data, "user_id");
            if (user && user->type == JSON_STRING) {
              json_object_set(node, "user_id", json_create_string(user->value.string));
            }
          }
          
          json_free(data);
        }
        
        json_array_append(nodes, node);
      }
    }
    
    /* Process OPERATION entries to find document relationships */
    if (strcmp(type, "OPERATION") == 0) {
      /* Parse data JSON */
      json_value_t* data = json_parse(data_json);
      if (!data) {
        free(line_copy);
        continue;
      }
      
      /* Get collection and document_id */
      json_value_t* coll_val = json_object_get(data, "collection");
      json_value_t* doc_val = json_object_get(data, "document_id");
      
      if (coll_val && coll_val->type == JSON_STRING && 
        doc_val && doc_val->type == JSON_STRING) {
        
        /* Check if this matches our filter */
        int matches_filter = 1;
        
        if (collection[0] && strcmp(collection, coll_val->value.string) != 0) {
          matches_filter = 0;
        }
        
        if (document_id[0] && strcmp(document_id, doc_val->value.string) != 0) {
          matches_filter = 0;
        }
        
        if (matches_filter) {
          const char* doc_collection = coll_val->value.string;
          const char* doc_id = doc_val->value.string;
          
          /* Check if document node already exists */
          int document_index = -1;
          for (int i = 0; i < document_count; i++) {
            if (document_collections[i] && document_ids[i] && 
              strcmp(document_collections[i], doc_collection) == 0 && 
              strcmp(document_ids[i], doc_id) == 0) {
              document_index = i;
              break;
            }
          }
          
          if (document_index == -1 && document_count < MAX_GRAPH_DOCUMENTS) {
            /* Add new document node */
            document_collections[document_count] = strdup(doc_collection);
            document_ids[document_count] = strdup(doc_id);
            
            if (!document_collections[document_count] || !document_ids[document_count]) {
              /* Clean up on error */
              if (document_collections[document_count]) {
                free(document_collections[document_count]);
                document_collections[document_count] = NULL;
              }
              if (document_ids[document_count]) {
                free(document_ids[document_count]);
                document_ids[document_count] = NULL;
              }
              
              json_free(data);
              free(line_copy);
              continue;
            }
            
            document_index = document_count++;
            
            /* Create node */
            json_value_t* node = json_create_object();
            if (node) {
              char doc_node_id[512];
              snprintf(doc_node_id, sizeof(doc_node_id), "%s_%s", doc_collection, doc_id);
              
              json_object_set(node, "id", json_create_string(doc_node_id));
              json_object_set(node, "type", json_create_string("document"));
              json_object_set(node, "collection", json_create_string(doc_collection));
              json_object_set(node, "document_id", json_create_string(doc_id));
              
              json_array_append(nodes, node);
            }
          }
          
          /* Add edge from transaction to document */
          if (transaction_index >= 0 && document_index >= 0) {
            json_value_t* edge = json_create_object();
            if (edge) {
              char edge_id[1024];
              char doc_node_id[512];
              snprintf(doc_node_id, sizeof(doc_node_id), "%s_%s", doc_collection, doc_id);
              snprintf(edge_id, sizeof(edge_id), "%s_to_%s", transaction_id, doc_node_id);
              
              json_object_set(edge, "id", json_create_string(edge_id));
              json_object_set(edge, "source", json_create_string(transaction_id));
              json_object_set(edge, "target", json_create_string(doc_node_id));
              
              /* Get operation type */
              json_value_t* op_type = json_object_get(data, "type");
              if (op_type && op_type->type == JSON_STRING) {
                json_object_set(edge, "operation", json_create_string(op_type->value.string));
              }
              
              json_object_set(edge, "timestamp", json_create_integer(timestamp));
              
              json_array_append(edges, edge);
            }
          }
        }
      }
      
      json_free(data);
    }
    
    free(line_copy);
  }
  
  fclose(file);
  pthread_mutex_unlock(&log->lock);
  
  /* Add nodes and edges to response */
  json_object_set(relationship_data, "nodes", nodes);
  json_object_set(relationship_data, "edges", edges);
  json_object_set(relationship_data, "node_count", json_create_integer(json_array_size(nodes)));
  json_object_set(relationship_data, "edge_count", json_create_integer(json_array_size(edges)));
  
  /* Clean up tracking arrays */
  for (int i = 0; i < transaction_count; i++) {
    if (transaction_ids[i]) {
      free(transaction_ids[i]);
    }
  }
  
  for (int i = 0; i < document_count; i++) {
    if (document_collections[i]) {
      free(document_collections[i]);
    }
    if (document_ids[i]) {
      free(document_ids[i]);
    }
  }
  
  /* Convert to response */
  char* json_str = json_stringify(relationship_data);
  json_free(relationship_data);
  
  if (!json_str) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to serialize relationship data\"}", "application/json");
  }
  
  http_response_t* response = create_http_response(HTTP_OK, json_str, "application/json");
  free(json_str);
  
  return response;
}