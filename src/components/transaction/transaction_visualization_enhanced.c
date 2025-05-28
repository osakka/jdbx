#include "transaction/transaction.h"
#include "api/api.h"
#include "utils/json.h"
#include "utils/json_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declarations for visualization helper functions */
static json_value_t* create_timeline_visualization(json_value_t* history, time_t start_time, time_t end_time);
static json_value_t* create_lifecycle_visualization(json_value_t* history);
static json_value_t* create_heatmap_visualization(json_value_t* history);
static json_value_t* create_distribution_visualization(json_value_t* history);
static json_value_t* create_dependency_visualization(json_value_t* history);
static json_value_t* create_sankey_visualization(json_value_t* history);

/* Helper function to convert enum to string */
static const char* __attribute__((unused)) visualization_format_to_string(visualization_format_t format) {
  switch (format) {
    case VISUALIZATION_FORMAT_TIMELINE:
      return "timeline";
    case VISUALIZATION_FORMAT_LIFECYCLE:
      return "lifecycle";
    case VISUALIZATION_FORMAT_HEATMAP:
      return "heatmap";
    case VISUALIZATION_FORMAT_DISTRIBUTION:
      return "distribution";
    case VISUALIZATION_FORMAT_DEPENDENCY:
      return "dependency";
    case VISUALIZATION_FORMAT_SANKEY:
      return "sankey";
    case VISUALIZATION_FORMAT_DEFAULT:
    default:
      return "default";
  }
}

/* Helper function to convert export format enum to string */
static const char* __attribute__((unused)) graph_export_format_to_string(graph_export_format_t format) {
  switch (format) {
    case EXPORT_FORMAT_DOT:
      return "dot";
    case EXPORT_FORMAT_GRAPHML:
      return "graphml";
    case EXPORT_FORMAT_CYTOSCAPE:
      return "cytoscape";
    case EXPORT_FORMAT_D3:
      return "d3";
    case EXPORT_FORMAT_JSON:
    default:
      return "json";
  }
}

/* Extract transaction history data for visualization */
json_value_t* transaction_get_history_data(transaction_manager_t* manager, time_t start_time, time_t end_time, 
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

/* Enhanced transaction history data for different visualization formats */
json_value_t* transaction_get_history_data_enhanced(transaction_manager_t* manager, time_t start_time, time_t end_time, 
                        const char* user_id, int limit, visualization_format_t format) {
  if (!manager) {
    return NULL;
  }
  
  /* First get the basic history data */
  json_value_t* history_data = transaction_get_history_data(manager, start_time, end_time, user_id, limit);
  if (!history_data) {
    return NULL;
  }
  
  /* Transform the data based on the requested visualization format */
  json_value_t* visualization_data = NULL;
  
  switch (format) {
    case VISUALIZATION_FORMAT_TIMELINE:
      visualization_data = create_timeline_visualization(history_data, start_time, end_time);
      break;
    case VISUALIZATION_FORMAT_LIFECYCLE:
      visualization_data = create_lifecycle_visualization(history_data);
      break;
    case VISUALIZATION_FORMAT_HEATMAP:
      visualization_data = create_heatmap_visualization(history_data);
      break;
    case VISUALIZATION_FORMAT_DISTRIBUTION:
      visualization_data = create_distribution_visualization(history_data);
      break;
    case VISUALIZATION_FORMAT_DEPENDENCY:
      visualization_data = create_dependency_visualization(history_data);
      break;
    case VISUALIZATION_FORMAT_SANKEY:
      visualization_data = create_sankey_visualization(history_data);
      break;
    case VISUALIZATION_FORMAT_DEFAULT:
    default:
      /* Return the basic history data for the default format */
      return history_data;
  }
  
  /* Free the original history data if we created a new format */
  if (visualization_data && visualization_data != history_data) {
    json_free(history_data);
  } else if (!visualization_data) {
    /* If the visualization creation failed, return the original data */
    return history_data;
  }
  
  return visualization_data;
}

/* Get transaction relationship data for graph visualization */
json_value_t* transaction_get_relationship_data(transaction_manager_t* manager, time_t start_time, time_t end_time,
                      const char* collection, const char* document_id) {
  if (!manager || !manager->log) {
    return NULL;
  }
  
  /* Create relationship graph structure */
  json_value_t* graph = json_create_object();
  if (!graph) {
    return NULL;
  }
  
  /* Create nodes and edges arrays */
  json_value_t* nodes = json_create_array();
  if (!nodes) {
    json_free(graph);
    return NULL;
  }
  
  json_value_t* edges = json_create_array();
  if (!edges) {
    json_free(nodes);
    json_free(graph);
    return NULL;
  }
  
  json_object_set(graph, "nodes", nodes);
  json_object_set(graph, "edges", edges);
  
  /* Maintain a map of transaction IDs to node indices */
  json_value_t* tx_map = json_create_object();
  if (!tx_map) {
    json_free(graph);
    return NULL;
  }
  
  /* Maintain a map of document IDs to node indices */
  json_value_t* doc_map = json_create_object();
  if (!doc_map) {
    json_free(tx_map);
    json_free(graph);
    return NULL;
  }
  
  /* Get transaction history data */
  json_value_t* history = transaction_get_history_data(manager, start_time, end_time, NULL, 1000);
  if (!history) {
    json_free(tx_map);
    json_free(doc_map);
    json_free(graph);
    return NULL;
  }
  
  /* Process history entries to build graph */
  int node_index = 0;
  
  for (size_t i = 0; i < json_array_size(history); i++) {
    json_value_t* entry = json_array_get(history, i);
    if (!entry || entry->type != JSON_OBJECT) {
      continue;
    }
    
    json_value_t* type = json_object_get(entry, "type");
    if (!type || type->type != JSON_STRING) {
      continue;
    }
    
    json_value_t* tx_id = json_object_get(entry, "transaction_id");
    if (!tx_id || tx_id->type != JSON_STRING) {
      continue;
    }
    
    /* Check if we're filtering by collection or document */
    if (strcmp(type->value.string, "OPERATION") == 0) {
      json_value_t* entry_collection = json_object_get(entry, "collection");
      json_value_t* entry_doc_id = json_object_get(entry, "document_id");
      
      /* Skip if we're filtering by collection and it doesn't match */
      if (collection && entry_collection && entry_collection->type == JSON_STRING && 
        strcmp(collection, entry_collection->value.string) != 0) {
        continue;
      }
      
      /* Skip if we're filtering by document and it doesn't match */
      if (document_id && entry_doc_id && entry_doc_id->type == JSON_STRING && 
        strcmp(document_id, entry_doc_id->value.string) != 0) {
        continue;
      }
      
      /* Add document node if it doesn't exist */
      if (entry_collection && entry_collection->type == JSON_STRING && 
        entry_doc_id && entry_doc_id->type == JSON_STRING) {
        char doc_key[512];
        snprintf(doc_key, sizeof(doc_key), "%s:%s", 
             entry_collection->value.string, entry_doc_id->value.string);
        
        json_value_t* doc_idx = json_object_get(doc_map, doc_key);
        if (!doc_idx) {
          /* Create document node */
          json_value_t* doc_node = json_create_object();
          json_object_set(doc_node, "id", json_create_integer(node_index));
          json_object_set(doc_node, "type", json_create_string("document"));
          json_object_set(doc_node, "collection", json_create_string(entry_collection->value.string));
          json_object_set(doc_node, "document_id", json_create_string(entry_doc_id->value.string));
          
          /* Add to nodes array */
          json_array_append(nodes, doc_node);
          
          /* Update document map */
          json_object_set(doc_map, doc_key, json_create_integer(node_index));
          
          /* Increment node index */
          node_index++;
        }
      }
      
      /* Add transaction node if it doesn't exist */
      json_value_t* tx_idx = json_object_get(tx_map, tx_id->value.string);
      if (!tx_idx) {
        /* Create transaction node */
        json_value_t* tx_node = json_create_object();
        json_object_set(tx_node, "id", json_create_integer(node_index));
        json_object_set(tx_node, "type", json_create_string("transaction"));
        json_object_set(tx_node, "transaction_id", json_create_string(tx_id->value.string));
        
        /* Add state if available */
        json_value_t* state = json_object_get(entry, "state");
        if (state && state->type == JSON_STRING) {
          json_object_set(tx_node, "state", json_create_string(state->value.string));
        }
        
        /* Add timestamp */
        json_value_t* timestamp = json_object_get(entry, "timestamp");
        if (timestamp && timestamp->type == JSON_INTEGER) {
          json_object_set(tx_node, "timestamp", json_create_integer(timestamp->value.integer));
        }
        
        /* Add to nodes array */
        json_array_append(nodes, tx_node);
        
        /* Update transaction map */
        json_object_set(tx_map, tx_id->value.string, json_create_integer(node_index));
        
        /* Increment node index */
        node_index++;
      }
      
      /* Add an edge between transaction and document */
      if (entry_collection && entry_collection->type == JSON_STRING && 
        entry_doc_id && entry_doc_id->type == JSON_STRING) {
        char doc_key[512];
        snprintf(doc_key, sizeof(doc_key), "%s:%s", 
             entry_collection->value.string, entry_doc_id->value.string);
        
        json_value_t* doc_idx = json_object_get(doc_map, doc_key);
        json_value_t* tx_idx = json_object_get(tx_map, tx_id->value.string);
        
        if (doc_idx && tx_idx) {
          /* Create edge */
          json_value_t* edge = json_create_object();
          json_object_set(edge, "source", json_create_integer(tx_idx->value.integer));
          json_object_set(edge, "target", json_create_integer(doc_idx->value.integer));
          
          /* Set edge type based on operation */
          json_value_t* operation = json_object_get(entry, "operation");
          if (operation && operation->type == JSON_STRING) {
            json_object_set(edge, "operation", json_create_string(operation->value.string));
            
            /* Add visual properties based on operation */
            if (strcmp(operation->value.string, "insert") == 0) {
              json_object_set(edge, "color", json_create_string("#4CAF50"));
            } else if (strcmp(operation->value.string, "update") == 0) {
              json_object_set(edge, "color", json_create_string("#2196F3"));
            } else if (strcmp(operation->value.string, "delete") == 0) {
              json_object_set(edge, "color", json_create_string("#F44336"));
            }
          }
          
          /* Add timestamp */
          json_value_t* timestamp = json_object_get(entry, "timestamp");
          if (timestamp && timestamp->type == JSON_INTEGER) {
            json_object_set(edge, "timestamp", json_create_integer(timestamp->value.integer));
          }
          
          /* Add to edges array */
          json_array_append(edges, edge);
        }
      }
    }
  }
  
  /* Add metadata to the graph */
  json_object_set(graph, "meta", json_create_object());
  json_value_t* meta = json_object_get(graph, "meta");
  if (meta) {
    json_object_set(meta, "start_time", json_create_integer(start_time));
    json_object_set(meta, "end_time", json_create_integer(end_time));
    json_object_set(meta, "node_count", json_create_integer(node_index));
    json_object_set(meta, "edge_count", json_create_integer(json_array_size(edges)));
    
    if (collection) {
      json_object_set(meta, "collection_filter", json_create_string(collection));
    }
    
    if (document_id) {
      json_object_set(meta, "document_id_filter", json_create_string(document_id));
    }
  }
  
  /* Clean up */
  json_free(tx_map);
  json_free(doc_map);
  json_free(history);
  
  return graph;
}

/* Export transaction graph in various formats */
char* transaction_graph_export(json_value_t* graph_data, graph_export_format_t format) {
  if (!graph_data) {
    return NULL;
  }
  
  /* Buffer for the output */
  char* result = NULL;
  
  switch (format) {
    case EXPORT_FORMAT_DOT:
      /* Generate GraphViz DOT format */
      result = calloc(1, 1024 * 1024); /* Allocate 1MB buffer */
      if (!result) {
        return NULL;
      }
      
      /* Write DOT header */
      strcpy(result, "digraph transaction_graph {\n");
      strcat(result, " graph [rankdir=LR, fontname=\"Helvetica\"];\n");
      strcat(result, " node [shape=box, style=filled, fontname=\"Helvetica\"];\n");
      strcat(result, " edge [fontname=\"Helvetica\"];\n\n");
      
      /* Get nodes and edges */
      json_value_t* nodes = json_object_get(graph_data, "nodes");
      json_value_t* edges = json_object_get(graph_data, "edges");
      
      if (nodes && nodes->type == JSON_ARRAY) {
        /* Write nodes */
        for (size_t i = 0; i < json_array_size(nodes); i++) {
          json_value_t* node = json_array_get(nodes, i);
          if (!node || node->type != JSON_OBJECT) {
            continue;
          }
          
          json_value_t* id = json_object_get(node, "id");
          json_value_t* type = json_object_get(node, "type");
          
          if (!id || id->type != JSON_INTEGER || !type || type->type != JSON_STRING) {
            continue;
          }
          
          char node_str[1024];
          
          if (strcmp(type->value.string, "transaction") == 0) {
            json_value_t* tx_id = json_object_get(node, "transaction_id");
            json_value_t* state = json_object_get(node, "state");
            
            if (tx_id && tx_id->type == JSON_STRING) {
              const char* state_str = state && state->type == JSON_STRING ? state->value.string : "unknown";
              const char* color = "lightblue";
              
              if (state && state->type == JSON_STRING) {
                if (strcmp(state->value.string, "committed") == 0) {
                  color = "lightgreen";
                } else if (strcmp(state->value.string, "aborted") == 0) {
                  color = "lightcoral";
                }
              }
              
              snprintf(node_str, sizeof(node_str), 
                   " node_%ld [label=\"TX: %s\\nState: %s\", fillcolor=\"%s\"];\n", 
                   id->value.integer, tx_id->value.string, state_str, color);
              strcat(result, node_str);
            }
          } else if (strcmp(type->value.string, "document") == 0) {
            json_value_t* coll = json_object_get(node, "collection");
            json_value_t* doc_id = json_object_get(node, "document_id");
            
            if (coll && coll->type == JSON_STRING && 
              doc_id && doc_id->type == JSON_STRING) {
              snprintf(node_str, sizeof(node_str), 
                   " node_%ld [label=\"Collection: %s\\nDoc ID: %s\", fillcolor=\"lightyellow\"];\n", 
                   id->value.integer, coll->value.string, doc_id->value.string);
              strcat(result, node_str);
            }
          }
        }
      }
      
      /* Add a newline between nodes and edges */
      strcat(result, "\n");
      
      if (edges && edges->type == JSON_ARRAY) {
        /* Write edges */
        for (size_t i = 0; i < json_array_size(edges); i++) {
          json_value_t* edge = json_array_get(edges, i);
          if (!edge || edge->type != JSON_OBJECT) {
            continue;
          }
          
          json_value_t* source = json_object_get(edge, "source");
          json_value_t* target = json_object_get(edge, "target");
          json_value_t* operation = json_object_get(edge, "operation");
          
          if (!source || source->type != JSON_INTEGER || 
            !target || target->type != JSON_INTEGER) {
            continue;
          }
          
          char edge_str[1024];
          const char* op_str = operation && operation->type == JSON_STRING ? 
                     operation->value.string : "access";
          const char* color = "#000000";
          
          if (operation && operation->type == JSON_STRING) {
            if (strcmp(operation->value.string, "insert") == 0) {
              color = "#4CAF50";
            } else if (strcmp(operation->value.string, "update") == 0) {
              color = "#2196F3";
            } else if (strcmp(operation->value.string, "delete") == 0) {
              color = "#F44336";
            }
          }
          
          snprintf(edge_str, sizeof(edge_str), 
               " node_%ld -> node_%ld [label=\"%s\", color=\"%s\"];\n", 
               source->value.integer, target->value.integer, op_str, color);
          strcat(result, edge_str);
        }
      }
      
      /* Close the DOT graph */
      strcat(result, "}\n");
      break;
      
    case EXPORT_FORMAT_GRAPHML:
      /* Generate GraphML XML format */
      result = calloc(1, 1024 * 1024); /* Allocate 1MB buffer */
      if (!result) {
        return NULL;
      }
      
      /* Write GraphML header */
      strcpy(result, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
      strcat(result, "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n");
      strcat(result, " <key id=\"type\" for=\"node\" attr.name=\"type\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"label\" for=\"node\" attr.name=\"label\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"transaction_id\" for=\"node\" attr.name=\"transaction_id\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"collection\" for=\"node\" attr.name=\"collection\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"document_id\" for=\"node\" attr.name=\"document_id\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"state\" for=\"node\" attr.name=\"state\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"timestamp\" for=\"node\" attr.name=\"timestamp\" attr.type=\"int\"/>\n");
      strcat(result, " <key id=\"operation\" for=\"edge\" attr.name=\"operation\" attr.type=\"string\"/>\n");
      strcat(result, " <key id=\"timestamp\" for=\"edge\" attr.name=\"timestamp\" attr.type=\"int\"/>\n");
      strcat(result, " <graph id=\"G\" edgedefault=\"directed\">\n");
      
      /* Get nodes and edges */
      json_value_t* nodes_xml = json_object_get(graph_data, "nodes");
      json_value_t* edges_xml = json_object_get(graph_data, "edges");
      
      if (nodes_xml && nodes_xml->type == JSON_ARRAY) {
        /* Write nodes */
        for (size_t i = 0; i < json_array_size(nodes_xml); i++) {
          json_value_t* node = json_array_get(nodes_xml, i);
          if (!node || node->type != JSON_OBJECT) {
            continue;
          }
          
          json_value_t* id = json_object_get(node, "id");
          json_value_t* type = json_object_get(node, "type");
          
          if (!id || id->type != JSON_INTEGER || !type || type->type != JSON_STRING) {
            continue;
          }
          
          char node_str[2048];
          snprintf(node_str, sizeof(node_str), "  <node id=\"n%ld\">\n", id->value.integer);
          strcat(result, node_str);
          
          snprintf(node_str, sizeof(node_str), "   <data key=\"type\">%s</data>\n", type->value.string);
          strcat(result, node_str);
          
          /* Add type-specific attributes */
          if (strcmp(type->value.string, "transaction") == 0) {
            json_value_t* tx_id = json_object_get(node, "transaction_id");
            if (tx_id && tx_id->type == JSON_STRING) {
              snprintf(node_str, sizeof(node_str), "   <data key=\"transaction_id\">%s</data>\n", tx_id->value.string);
              strcat(result, node_str);
              
              snprintf(node_str, sizeof(node_str), "   <data key=\"label\">TX: %s</data>\n", tx_id->value.string);
              strcat(result, node_str);
            }
            
            json_value_t* state = json_object_get(node, "state");
            if (state && state->type == JSON_STRING) {
              snprintf(node_str, sizeof(node_str), "   <data key=\"state\">%s</data>\n", state->value.string);
              strcat(result, node_str);
            }
          } else if (strcmp(type->value.string, "document") == 0) {
            json_value_t* coll = json_object_get(node, "collection");
            json_value_t* doc_id = json_object_get(node, "document_id");
            
            if (coll && coll->type == JSON_STRING) {
              snprintf(node_str, sizeof(node_str), "   <data key=\"collection\">%s</data>\n", coll->value.string);
              strcat(result, node_str);
            }
            
            if (doc_id && doc_id->type == JSON_STRING) {
              snprintf(node_str, sizeof(node_str), "   <data key=\"document_id\">%s</data>\n", doc_id->value.string);
              strcat(result, node_str);
              
              if (coll && coll->type == JSON_STRING) {
                snprintf(node_str, sizeof(node_str), "   <data key=\"label\">%s: %s</data>\n", 
                     coll->value.string, doc_id->value.string);
                strcat(result, node_str);
              } else {
                snprintf(node_str, sizeof(node_str), "   <data key=\"label\">Doc: %s</data>\n", doc_id->value.string);
                strcat(result, node_str);
              }
            }
          }
          
          /* Add timestamp if available */
          json_value_t* timestamp = json_object_get(node, "timestamp");
          if (timestamp && timestamp->type == JSON_INTEGER) {
            snprintf(node_str, sizeof(node_str), "   <data key=\"timestamp\">%ld</data>\n", timestamp->value.integer);
            strcat(result, node_str);
          }
          
          strcat(result, "  </node>\n");
        }
      }
      
      if (edges_xml && edges_xml->type == JSON_ARRAY) {
        /* Write edges */
        for (size_t i = 0; i < json_array_size(edges_xml); i++) {
          json_value_t* edge = json_array_get(edges_xml, i);
          if (!edge || edge->type != JSON_OBJECT) {
            continue;
          }
          
          json_value_t* source = json_object_get(edge, "source");
          json_value_t* target = json_object_get(edge, "target");
          
          if (!source || source->type != JSON_INTEGER || 
            !target || target->type != JSON_INTEGER) {
            continue;
          }
          
          char edge_str[1024];
          snprintf(edge_str, sizeof(edge_str), "  <edge id=\"e%lu\" source=\"n%ld\" target=\"n%ld\">\n", 
               (unsigned long)i, source->value.integer, target->value.integer);
          strcat(result, edge_str);
          
          /* Add operation if available */
          json_value_t* operation = json_object_get(edge, "operation");
          if (operation && operation->type == JSON_STRING) {
            snprintf(edge_str, sizeof(edge_str), "   <data key=\"operation\">%s</data>\n", operation->value.string);
            strcat(result, edge_str);
          }
          
          /* Add timestamp if available */
          json_value_t* timestamp = json_object_get(edge, "timestamp");
          if (timestamp && timestamp->type == JSON_INTEGER) {
            snprintf(edge_str, sizeof(edge_str), "   <data key=\"timestamp\">%ld</data>\n", timestamp->value.integer);
            strcat(result, edge_str);
          }
          
          strcat(result, "  </edge>\n");
        }
      }
      
      /* Close the GraphML file */
      strcat(result, " </graph>\n");
      strcat(result, "</graphml>\n");
      break;
      
    case EXPORT_FORMAT_CYTOSCAPE:
    case EXPORT_FORMAT_D3:
      /* Generate Cytoscape/D3 compatible JSON format (very similar) */
      {
        /* Create a new JSON structure */
        json_value_t* export_json = json_create_object();
        json_value_t* elements = json_create_object();
        json_object_set(export_json, "elements", elements);
        
        /* Create arrays for nodes and edges */
        json_value_t* nodes_array = json_create_array();
        json_value_t* edges_array = json_create_array();
        json_object_set(elements, "nodes", nodes_array);
        json_object_set(elements, "edges", edges_array);
        
        /* Get nodes and edges from graph data */
        json_value_t* nodes_json = json_object_get(graph_data, "nodes");
        json_value_t* edges_json = json_object_get(graph_data, "edges");
        
        if (nodes_json && nodes_json->type == JSON_ARRAY) {
          /* Convert nodes */
          for (size_t i = 0; i < json_array_size(nodes_json); i++) {
            json_value_t* node = json_array_get(nodes_json, i);
            if (!node || node->type != JSON_OBJECT) {
              continue;
            }
            
            json_value_t* id = json_object_get(node, "id");
            if (!id || id->type != JSON_INTEGER) {
              continue;
            }
            
            /* Create a new node with cytoscape format */
            json_value_t* cy_node = json_create_object();
            json_value_t* data = json_create_object();
            json_object_set(cy_node, "data", data);
            
            /* Set id */
            char id_str[32];
            snprintf(id_str, sizeof(id_str), "n%ld", id->value.integer);
            json_object_set(data, "id", json_create_string(id_str));
            
            /* Copy all other properties */
            json_value_t* type = json_object_get(node, "type");
            if (type && type->type == JSON_STRING) {
              json_object_set(data, "type", json_create_string(type->value.string));
              
              /* Add type-specific styling */
              if (strcmp(type->value.string, "transaction") == 0) {
                json_object_set(data, "shape", json_create_string("rectangle"));
                
                json_value_t* state = json_object_get(node, "state");
                if (state && state->type == JSON_STRING) {
                  if (strcmp(state->value.string, "committed") == 0) {
                    json_object_set(data, "color", json_create_string("#4CAF50"));
                  } else if (strcmp(state->value.string, "aborted") == 0) {
                    json_object_set(data, "color", json_create_string("#F44336"));
                  } else {
                    json_object_set(data, "color", json_create_string("#2196F3"));
                  }
                } else {
                  json_object_set(data, "color", json_create_string("#2196F3"));
                }
              } else if (strcmp(type->value.string, "document") == 0) {
                json_object_set(data, "shape", json_create_string("ellipse"));
                json_object_set(data, "color", json_create_string("#FFC107"));
              }
            }
            
            /* Create label */
            if (strcmp(type->value.string, "transaction") == 0) {
              json_value_t* tx_id = json_object_get(node, "transaction_id");
              if (tx_id && tx_id->type == JSON_STRING) {
                char label[128];
                snprintf(label, sizeof(label), "TX: %s", tx_id->value.string);
                json_object_set(data, "label", json_create_string(label));
                json_object_set(data, "transaction_id", json_create_string(tx_id->value.string));
              }
            } else if (strcmp(type->value.string, "document") == 0) {
              json_value_t* coll = json_object_get(node, "collection");
              json_value_t* doc_id = json_object_get(node, "document_id");
              
              if (coll && coll->type == JSON_STRING && 
                doc_id && doc_id->type == JSON_STRING) {
                char label[256];
                snprintf(label, sizeof(label), "%s: %s", 
                     coll->value.string, doc_id->value.string);
                json_object_set(data, "label", json_create_string(label));
                json_object_set(data, "collection", json_create_string(coll->value.string));
                json_object_set(data, "document_id", json_create_string(doc_id->value.string));
              }
            }
            
            /* Copy any additional properties */
            const char* keys[] = { "state", "timestamp" };
            for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
              json_value_t* prop = json_object_get(node, keys[k]);
              if (prop) {
                if (prop->type == JSON_STRING) {
                  json_object_set(data, keys[k], json_create_string(prop->value.string));
                } else if (prop->type == JSON_INTEGER) {
                  json_object_set(data, keys[k], json_create_integer(prop->value.integer));
                }
              }
            }
            
            /* Add to nodes array */
            json_array_append(nodes_array, cy_node);
          }
        }
        
        if (edges_json && edges_json->type == JSON_ARRAY) {
          /* Convert edges */
          for (size_t i = 0; i < json_array_size(edges_json); i++) {
            json_value_t* edge = json_array_get(edges_json, i);
            if (!edge || edge->type != JSON_OBJECT) {
              continue;
            }
            
            json_value_t* source = json_object_get(edge, "source");
            json_value_t* target = json_object_get(edge, "target");
            
            if (!source || source->type != JSON_INTEGER || 
              !target || target->type != JSON_INTEGER) {
              continue;
            }
            
            /* Create a new edge with cytoscape format */
            json_value_t* cy_edge = json_create_object();
            json_value_t* data = json_create_object();
            json_object_set(cy_edge, "data", data);
            
            /* Set id, source and target */
            char id_str[32], source_str[32], target_str[32];
            snprintf(id_str, sizeof(id_str), "e%lu", (unsigned long)i);
            snprintf(source_str, sizeof(source_str), "n%ld", source->value.integer);
            snprintf(target_str, sizeof(target_str), "n%ld", target->value.integer);
            
            json_object_set(data, "id", json_create_string(id_str));
            json_object_set(data, "source", json_create_string(source_str));
            json_object_set(data, "target", json_create_string(target_str));
            
            /* Copy operation and add style */
            json_value_t* operation = json_object_get(edge, "operation");
            if (operation && operation->type == JSON_STRING) {
              json_object_set(data, "operation", json_create_string(operation->value.string));
              json_object_set(data, "label", json_create_string(operation->value.string));
              
              /* Set color based on operation */
              if (strcmp(operation->value.string, "insert") == 0) {
                json_object_set(data, "color", json_create_string("#4CAF50"));
                json_object_set(data, "width", json_create_integer(3));
              } else if (strcmp(operation->value.string, "update") == 0) {
                json_object_set(data, "color", json_create_string("#2196F3"));
                json_object_set(data, "width", json_create_integer(2));
              } else if (strcmp(operation->value.string, "delete") == 0) {
                json_object_set(data, "color", json_create_string("#F44336"));
                json_object_set(data, "width", json_create_integer(3));
                json_object_set(data, "line-style", json_create_string("dashed"));
              }
            }
            
            /* Copy timestamp */
            json_value_t* timestamp = json_object_get(edge, "timestamp");
            if (timestamp && timestamp->type == JSON_INTEGER) {
              json_object_set(data, "timestamp", json_create_integer(timestamp->value.integer));
            }
            
            /* Add to edges array */
            json_array_append(edges_array, cy_edge);
          }
        }
        
        /* Add layout and style information for D3 */
        if (format == EXPORT_FORMAT_D3) {
          json_object_set(export_json, "style", json_create_array());
          json_value_t* styles = json_object_get(export_json, "style");
          
          /* Add node styles */
          json_value_t* node_style = json_create_object();
          json_object_set(node_style, "selector", json_create_string("node"));
          json_value_t* node_style_props = json_create_object();
          json_object_set(node_style_props, "font-size", json_create_string("12px"));
          json_object_set(node_style_props, "text-valign", json_create_string("center"));
          json_object_set(node_style_props, "text-halign", json_create_string("center"));
          json_object_set(node_style, "style", node_style_props);
          json_array_append(styles, node_style);
          
          /* Add edge styles */
          json_value_t* edge_style = json_create_object();
          json_object_set(edge_style, "selector", json_create_string("edge"));
          json_value_t* edge_style_props = json_create_object();
          json_object_set(edge_style_props, "width", json_create_string("2px"));
          json_object_set(edge_style_props, "curve-style", json_create_string("bezier"));
          json_object_set(edge_style_props, "target-arrow-shape", json_create_string("triangle"));
          json_object_set(edge_style, "style", edge_style_props);
          json_array_append(styles, edge_style);
        }
        
        /* Convert to JSON string */
        result = json_stringify(export_json);
        
        /* Clean up */
        json_free(export_json);
      }
      break;
      
    case EXPORT_FORMAT_JSON:
    default:
      /* Just return the JSON string representation of the graph data */
      result = json_stringify(graph_data);
      break;
  }
  
  return result;
}

/* Timeline visualization helper */
static json_value_t* create_timeline_visualization(json_value_t* history, time_t start_time __attribute__((unused)), time_t end_time __attribute__((unused))) {
  if (!history || history->type != JSON_ARRAY) {
    return NULL;
  }
  
  /* Create a new array for the timeline */
  json_value_t* timeline = json_create_array();
  if (!timeline) {
    return NULL;
  }
  
  /* Process each entry in the history */
  for (size_t i = 0; i < json_array_size(history); i++) {
    json_value_t* entry = json_array_get(history, i);
    if (!entry || entry->type != JSON_OBJECT) {
      continue;
    }
    
    /* Create a new timeline entry */
    json_value_t* timeline_entry = json_create_object();
    if (!timeline_entry) {
      continue;
    }
    
    /* Copy basic properties */
    json_value_t* timestamp = json_object_get(entry, "timestamp");
    json_value_t* type = json_object_get(entry, "type");
    json_value_t* tx_id = json_object_get(entry, "transaction_id");
    
    if (timestamp && timestamp->type == JSON_INTEGER) {
      json_object_set(timeline_entry, "timestamp", json_create_integer(timestamp->value.integer));
    }
    
    if (type && type->type == JSON_STRING) {
      json_object_set(timeline_entry, "event_type", json_create_string(type->value.string));
    }
    
    if (tx_id && tx_id->type == JSON_STRING) {
      json_object_set(timeline_entry, "transaction_id", json_create_string(tx_id->value.string));
    }
    
    /* Add additional details based on entry type */
    if (type && type->type == JSON_STRING) {
      if (strcmp(type->value.string, "STATE") == 0) {
        json_value_t* state = json_object_get(entry, "state");
        if (state && state->type == JSON_STRING) {
          json_object_set(timeline_entry, "state", json_create_string(state->value.string));
          
          /* Set event_name based on state */
          char event_name[128];
          if (strcmp(state->value.string, "active") == 0) {
            strcpy(event_name, "Transaction Started");
          } else if (strcmp(state->value.string, "committed") == 0) {
            strcpy(event_name, "Transaction Committed");
          } else if (strcmp(state->value.string, "aborted") == 0) {
            strcpy(event_name, "Transaction Aborted");
          } else {
            snprintf(event_name, sizeof(event_name), "Transaction %s", state->value.string);
          }
          json_object_set(timeline_entry, "event_name", json_create_string(event_name));
        }
      } else if (strcmp(type->value.string, "OPERATION") == 0) {
        json_value_t* operation = json_object_get(entry, "operation");
        json_value_t* collection = json_object_get(entry, "collection");
        json_value_t* document_id = json_object_get(entry, "document_id");
        
        if (operation && operation->type == JSON_STRING) {
          json_object_set(timeline_entry, "operation", json_create_string(operation->value.string));
          
          /* Create event_name */
          char event_name[256];
          const char* op_str = operation->value.string;
          const char* coll_str = collection && collection->type == JSON_STRING ? 
                      collection->value.string : "unknown";
          const char* doc_str = document_id && document_id->type == JSON_STRING ? 
                     document_id->value.string : "unknown";
          
          snprintf(event_name, sizeof(event_name), "%s %s/%s", 
               op_str, coll_str, doc_str);
          json_object_set(timeline_entry, "event_name", json_create_string(event_name));
        }
        
        if (collection && collection->type == JSON_STRING) {
          json_object_set(timeline_entry, "collection", json_create_string(collection->value.string));
        }
        
        if (document_id && document_id->type == JSON_STRING) {
          json_object_set(timeline_entry, "document_id", json_create_string(document_id->value.string));
        }
      }
    }
    
    /* Add to timeline */
    json_array_append(timeline, timeline_entry);
  }
  
  return timeline;
}

/* Lifecycle visualization helper */
static json_value_t* create_lifecycle_visualization(json_value_t* history) {
  if (!history || history->type != JSON_ARRAY) {
    return NULL;
  }
  
  /* Create a map of transaction_id to transaction object */
  json_value_t* transactions = json_create_object();
  if (!transactions) {
    return NULL;
  }
  
  /* Process each entry in the history */
  for (size_t i = 0; i < json_array_size(history); i++) {
    json_value_t* entry = json_array_get(history, i);
    if (!entry || entry->type != JSON_OBJECT) {
      continue;
    }
    
    json_value_t* tx_id = json_object_get(entry, "transaction_id");
    json_value_t* timestamp = json_object_get(entry, "timestamp");
    json_value_t* type = json_object_get(entry, "type");
    
    if (!tx_id || tx_id->type != JSON_STRING || 
      !timestamp || timestamp->type != JSON_INTEGER ||
      !type || type->type != JSON_STRING) {
      continue;
    }
    
    /* Get or create transaction object */
    json_value_t* tx = json_object_get(transactions, tx_id->value.string);
    if (!tx) {
      tx = json_create_object();
      if (!tx) {
        continue;
      }
      
      /* Initialize transaction object */
      json_object_set(tx, "transaction_id", json_create_string(tx_id->value.string));
      json_object_set(tx, "operations", json_create_array());
      json_object_set(tx, "start_time", json_create_integer(timestamp->value.integer));
      json_object_set(tx, "state", json_create_string("unknown"));
      json_object_set(tx, "events", json_create_array());
      
      /* Add to transactions map */
      json_object_set(transactions, tx_id->value.string, tx);
    }
    
    /* Update transaction based on entry type */
    if (strcmp(type->value.string, "STATE") == 0) {
      json_value_t* state = json_object_get(entry, "state");
      if (state && state->type == JSON_STRING) {
        json_object_set(tx, "state", json_create_string(state->value.string));
        
        /* If committed, set end_time */
        if (strcmp(state->value.string, "committed") == 0 || 
          strcmp(state->value.string, "aborted") == 0) {
          json_object_set(tx, "end_time", json_create_integer(timestamp->value.integer));
        }
      }
      
      /* Add event */
      json_value_t* events = json_object_get(tx, "events");
      if (events && events->type == JSON_ARRAY) {
        json_value_t* event = json_create_object();
        json_object_set(event, "timestamp", json_create_integer(timestamp->value.integer));
        json_object_set(event, "type", json_create_string("state_change"));
        if (state && state->type == JSON_STRING) {
          json_object_set(event, "state", json_create_string(state->value.string));
        }
        json_array_append(events, event);
      }
    } else if (strcmp(type->value.string, "OPERATION") == 0) {
      json_value_t* operation = json_object_get(entry, "operation");
      json_value_t* collection = json_object_get(entry, "collection");
      json_value_t* document_id = json_object_get(entry, "document_id");
      
      if (operation && operation->type == JSON_STRING) {
        /* Add to operations array */
        json_value_t* operations = json_object_get(tx, "operations");
        if (operations && operations->type == JSON_ARRAY) {
          json_value_t* op = json_create_object();
          json_object_set(op, "timestamp", json_create_integer(timestamp->value.integer));
          json_object_set(op, "operation", json_create_string(operation->value.string));
          
          if (collection && collection->type == JSON_STRING) {
            json_object_set(op, "collection", json_create_string(collection->value.string));
          }
          
          if (document_id && document_id->type == JSON_STRING) {
            json_object_set(op, "document_id", json_create_string(document_id->value.string));
          }
          
          json_array_append(operations, op);
        }
        
        /* Add event */
        json_value_t* events = json_object_get(tx, "events");
        if (events && events->type == JSON_ARRAY) {
          json_value_t* event = json_create_object();
          json_object_set(event, "timestamp", json_create_integer(timestamp->value.integer));
          json_object_set(event, "type", json_create_string("operation"));
          json_object_set(event, "operation", json_create_string(operation->value.string));
          
          if (collection && collection->type == JSON_STRING) {
            json_object_set(event, "collection", json_create_string(collection->value.string));
          }
          
          if (document_id && document_id->type == JSON_STRING) {
            json_object_set(event, "document_id", json_create_string(document_id->value.string));
          }
          
          json_array_append(events, event);
        }
      }
    }
  }
  
  /* Convert transactions object to array */
  json_value_t* result = json_create_array();
  if (!result) {
    json_free(transactions);
    return NULL;
  }
  
  /* For each transaction in the map */
  int obj_count = json_object_size(transactions);
  char** keys = malloc(obj_count * sizeof(char*));
  if (!keys) {
    json_free(transactions);
    json_free(result);
    return NULL;
  }
  
  json_object_keys(transactions, keys, obj_count);
  
  for (int i = 0; i < obj_count; i++) {
    json_value_t* tx = json_object_get(transactions, keys[i]);
    if (tx && tx->type == JSON_OBJECT) {
      /* Calculate duration if possible */
      json_value_t* start_time = json_object_get(tx, "start_time");
      json_value_t* end_time = json_object_get(tx, "end_time");
      
      if (start_time && start_time->type == JSON_INTEGER && 
        end_time && end_time->type == JSON_INTEGER) {
        json_object_set(tx, "duration", 
               json_create_integer(end_time->value.integer - start_time->value.integer));
      }
      
      /* Add to result array */
      json_array_append(result, json_clone(tx));
    }
    
    free(keys[i]);
  }
  
  free(keys);
  json_free(transactions);
  
  return result;
}

/* Simple stub implementations of the remaining visualization formats */

static json_value_t* create_heatmap_visualization(json_value_t* history __attribute__((unused))) {
  /* Create a basic heatmap data structure */
  json_value_t* heatmap = json_create_object();
  json_object_set(heatmap, "type", json_create_string("heatmap"));
  json_object_set(heatmap, "data", json_create_array());
  return heatmap;
}

static json_value_t* create_distribution_visualization(json_value_t* history __attribute__((unused))) {
  /* Create a basic distribution data structure */
  json_value_t* distribution = json_create_object();
  json_object_set(distribution, "type", json_create_string("distribution"));
  json_object_set(distribution, "data", json_create_array());
  return distribution;
}

static json_value_t* create_dependency_visualization(json_value_t* history __attribute__((unused))) {
  /* Create a basic dependency graph data structure */
  json_value_t* dependency = json_create_object();
  json_object_set(dependency, "type", json_create_string("dependency"));
  json_object_set(dependency, "nodes", json_create_array());
  json_object_set(dependency, "edges", json_create_array());
  return dependency;
}

static json_value_t* create_sankey_visualization(json_value_t* history __attribute__((unused))) {
  /* Create a basic Sankey diagram data structure */
  json_value_t* sankey = json_create_object();
  json_object_set(sankey, "type", json_create_string("sankey"));
  json_object_set(sankey, "nodes", json_create_array());
  json_object_set(sankey, "links", json_create_array());
  return sankey;
}

/* Suppress unused function warnings by referencing them */
static void __attribute__((unused)) suppress_unused_function_warnings(void) {
  if (0) {
    visualization_format_to_string(VISUALIZATION_FORMAT_DEFAULT);
    graph_export_format_to_string(EXPORT_FORMAT_DOT);
  }
}