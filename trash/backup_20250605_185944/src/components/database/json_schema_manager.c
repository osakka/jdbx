#include "database/database.h"
#include "utils/json.h"
#include "utils/logger.h"
#include <string.h>
#include <stdlib.h>

/* JSON Schema collection name */
#define SCHEMAS_COLLECTION "_schemas"

/* Store a JSON schema for a collection */
int db_store_json_schema(database_t* db, const char* collection_name, json_value_t* schema) {
  if (!db || !collection_name || !schema) {
    LOG_ERROR("Invalid parameters for storing JSON schema");
    return 0;
  }
  
  /* Create schema document */
  json_value_t* schema_doc = json_create_object();
  if (!schema_doc) {
    LOG_ERROR("create schema document");
    return 0;
  }
  
  /* Add fields */
  json_object_set(schema_doc, "collection", json_create_string(collection_name));
  json_object_set(schema_doc, "schema", json_clone(schema));
  
  /* Add timestamp */
  char timestamp[64];
  time_t now = time(NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
  json_object_set(schema_doc, "updated_at", json_create_string(timestamp));
  
  /* Check if _schemas collection exists */
  if (!db_collection_exists(db, SCHEMAS_COLLECTION)) {
    if (!db_create_collection(db, SCHEMAS_COLLECTION)) {
      LOG_ERROR("create schemas collection");
      json_free(schema_doc);
      return 0;
    }
  }
  
  /* Delete existing schema for this collection if any */
  json_value_t* query = json_create_object();
  json_object_set(query, "collection", json_create_string(collection_name));
  
  /* Query for existing schema */
  json_value_t* existing = db_query_documents(db, SCHEMAS_COLLECTION, query);
  if (existing) {
    /* Extract documents array */
    json_value_t* docs = json_object_get(existing, "documents");
    if (docs && docs->type == JSON_ARRAY && json_array_size(docs) > 0) {
      /* Delete existing schema */
      json_value_t* old_doc = json_array_get(docs, 0);
    json_value_t* id_val = json_object_get(old_doc, "_id");
    if (id_val && id_val->type == JSON_STRING) {
      db_delete_document(db, SCHEMAS_COLLECTION, id_val->value.string);
    }
    }
  }
  
  if (existing) json_free(existing);
  json_free(query);
  
  /* Insert new schema */
  json_value_t* result = db_insert_document(db, SCHEMAS_COLLECTION, schema_doc);
  json_free(schema_doc);
  
  if (!result) {
    LOG_ERROR("store schema for collection: %s");
    return 0;
  }
  
  json_free(result);
  LOG_INFO("Schema stored for collection: %s");
  return 1;
}

/* Retrieve JSON schema for a collection */
json_value_t* db_get_json_schema(database_t* db, const char* collection_name) {
  if (!db || !collection_name) {
    return NULL;
  }
  
  /* Query for schema */
  json_value_t* query = json_create_object();
  json_object_set(query, "collection", json_create_string(collection_name));
  
  json_value_t* results = db_query_documents(db, SCHEMAS_COLLECTION, query);
  json_free(query);
  
  if (!results) {
    return NULL;
  }
  
  /* Extract documents array from results */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    json_free(results);
    return NULL;
  }
  
  /* Get first result */
  json_value_t* schema_doc = json_array_get(documents, 0);
  json_value_t* schema = json_object_get(schema_doc, "schema");
  
  /* Clone the schema before freeing results */
  json_value_t* schema_copy = schema ? json_clone(schema) : NULL;
  json_free(results);
  
  return schema_copy;
}

/* Delete JSON schema for a collection */
int db_delete_json_schema(database_t* db, const char* collection_name) {
  if (!db || !collection_name) {
    return 0;
  }
  
  /* Query for schema */
  json_value_t* query = json_create_object();
  json_object_set(query, "collection", json_create_string(collection_name));
  
  json_value_t* results = db_query_documents(db, SCHEMAS_COLLECTION, query);
  json_free(query);
  
  if (!results) {
    return 1; /* Already deleted */
  }
  
  /* Extract documents array from results */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY || json_array_size(documents) == 0) {
    json_free(results);
    return 1; /* Already deleted */
  }
  
  /* Delete all matching schemas */
  int success = 1;
  for (size_t i = 0; i < json_array_size(documents); i++) {
    json_value_t* doc = json_array_get(documents, i);
    json_value_t* id_val = json_object_get(doc, "_id");
    if (id_val && id_val->type == JSON_STRING) {
      if (!db_delete_document(db, SCHEMAS_COLLECTION, id_val->value.string)) {
        success = 0;
      }
    }
  }
  
  json_free(results);
  return success;
}

/* List all schemas */
json_value_t* db_list_json_schemas(database_t* db) {
  if (!db) {
    return NULL;
  }
  
  /* Check if schemas collection exists */
  if (!db_collection_exists(db, SCHEMAS_COLLECTION)) {
    /* Return empty array */
    return json_create_array();
  }
  
  /* Get all schemas */
  json_value_t* empty_query = json_create_object();
  json_value_t* results = db_query_documents(db, SCHEMAS_COLLECTION, empty_query);
  json_free(empty_query);
  
  if (!results) {
    return json_create_array();
  }
  
  /* Extract documents array from results */
  json_value_t* documents = json_object_get(results, "documents");
  if (!documents || documents->type != JSON_ARRAY) {
    json_free(results);
    return json_create_array();
  }
  
  /* Clone the documents array to return it */
  json_value_t* schemas_array = json_clone(documents);
  json_free(results);
  
  return schemas_array ? schemas_array : json_create_array();
}

/* Validate a document against a JSON Schema */
int db_validate_json_schema(json_value_t* schema, json_value_t* document, char** error_msg) {
  if (!schema || !document) {
    if (error_msg) *error_msg = strdup("Invalid parameters");
    return 0;
  }
  
  /* Basic JSON Schema validation implementation */
  /* For now, we'll do basic type checking */
  
  /* Get type from schema */
  json_value_t* type_val = json_object_get(schema, "type");
  if (!type_val || type_val->type != JSON_STRING) {
    if (error_msg) *error_msg = strdup("Schema missing 'type' field");
    return 0;
  }
  
  const char* expected_type = type_val->value.string;
  
  /* Check document type matches */
  if (strcmp(expected_type, "object") == 0 && document->type != JSON_OBJECT) {
    if (error_msg) *error_msg = strdup("Document must be an object");
    return 0;
  } else if (strcmp(expected_type, "array") == 0 && document->type != JSON_ARRAY) {
    if (error_msg) *error_msg = strdup("Document must be an array");
    return 0;
  } else if (strcmp(expected_type, "string") == 0 && document->type != JSON_STRING) {
    if (error_msg) *error_msg = strdup("Document must be a string");
    return 0;
  } else if (strcmp(expected_type, "number") == 0 && document->type != JSON_NUMBER) {
    if (error_msg) *error_msg = strdup("Document must be a number");
    return 0;
  } else if (strcmp(expected_type, "boolean") == 0 && document->type != JSON_BOOLEAN) {
    if (error_msg) *error_msg = strdup("Document must be a boolean");
    return 0;
  }
  
  /* For object types, validate properties */
  if (strcmp(expected_type, "object") == 0) {
    json_value_t* properties = json_object_get(schema, "properties");
    if (properties && properties->type == JSON_OBJECT) {
      /* Check required properties */
      json_value_t* required = json_object_get(schema, "required");
      if (required && required->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(required); i++) {
          json_value_t* req_prop = json_array_get(required, i);
          if (req_prop && req_prop->type == JSON_STRING) {
            const char* prop_name = req_prop->value.string;
            if (!json_object_get(document, prop_name)) {
              if (error_msg) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Missing required property: %s", prop_name);
                *error_msg = strdup(msg);
              }
              return 0;
            }
          }
        }
      }
      
      /* Validate each property that exists in the document */
      const char* key;
      json_value_t* value;
      
      json_object_foreach(document, key, value) {
        json_value_t* prop_schema = json_object_get(properties, key);
        if (prop_schema) {
          /* Recursively validate the property */
          char* prop_error = NULL;
          if (!db_validate_json_schema(prop_schema, value, &prop_error)) {
            if (error_msg) {
              char msg[512];
              snprintf(msg, sizeof(msg), "Property '%s': %s", key, 
                  prop_error ? prop_error : "validation failed");
              *error_msg = strdup(msg);
            }
            if (prop_error) free(prop_error);
            return 0;
          }
        }
      }
    }
  }
  
  return 1; /* Validation passed */
}