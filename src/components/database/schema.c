#include "database/database.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/* Create a new schema */
schema_t* db_create_schema(const char* name, const char* description) {
  LOG_DEBUG("Creating schema with name: %s", name ? name : "NULL");

  if (!name) {
    LOG_ERROR("Cannot create schema: name cannot be NULL.");
    return NULL;
  }

  schema_t* schema = (schema_t*)BUFFER_ALLOC(sizeof(schema_t));
  if (!schema) {
    LOG_ERROR("Cannot allocate memory for schema '%s'.", name);
    return NULL;
  }

  schema->name = BUFFER_STRDUP(name);
  schema->description = description ? BUFFER_STRDUP(description) : NULL;
  schema->rules = NULL;

  LOG_INFO("Schema '%s' created.", name);
  return schema;
}

/* Free schema and all resources */
void db_free_schema(schema_t* schema) {
  if (!schema) {
    LOG_DEBUG("Attempt to free NULL schema, ignoring.");
    return;
  }

  LOG_DEBUG("Freeing schema '%s'.", schema->name ? schema->name : "unnamed");

  /* Count rules for logging */
  int rule_count = 0;
  schema_rule_t* counting = schema->rules;
  while (counting) {
    rule_count++;
    counting = counting->next;
  }

  /* Free schema rules */
  TRACE_DB("Freeing %d rules for schema '%s'", rule_count, schema->name ? schema->name : "unnamed");
  schema_rule_t* current = schema->rules;
  while (current) {
    schema_rule_t* next = current->next;
    db_free_schema_rule(current);
    current = next;
  }

  /* Free schema name and description */
  if (schema->name) {
    TRACE_DB("Freeing schema name: %s", schema->name);
    BUFFER_FREE(schema->name);
  }

  if (schema->description) {
    TRACE_DB("Freeing schema description.");
    BUFFER_FREE(schema->description);
  }

  /* Free schema structure */
  TRACE_DB("Freeing schema structure.");
  BUFFER_FREE(schema);

  LOG_DEBUG("Schema freed successfully.");
}

/* Create a schema rule */
schema_rule_t* db_create_schema_rule(schema_rule_type_t type, const char* field_path) {
  const char* type_str = "unknown";
  switch (type) {
    case SCHEMA_TYPE_CHECK: type_str = "type"; break;
    case SCHEMA_REQUIRED: type_str = "required"; break;
    case SCHEMA_MIN_LENGTH: type_str = "minLength"; break;
    case SCHEMA_MAX_LENGTH: type_str = "maxLength"; break;
    case SCHEMA_MIN_VALUE: type_str = "minValue"; break;
    case SCHEMA_MAX_VALUE: type_str = "maxValue"; break;
    case SCHEMA_PATTERN: type_str = "pattern"; break;
    case SCHEMA_ENUM: type_str = "enum"; break;
    case SCHEMA_NESTED: type_str = "nested"; break;
  }

  LOG_DEBUG("Creating schema rule of type '%s' for field path: %s",
       type_str, field_path ? field_path : "NULL");

  if (!field_path) {
    LOG_ERROR("Cannot create schema rule: field_path cannot be NULL.");
    return NULL;
  }

  schema_rule_t* rule = (schema_rule_t*)BUFFER_ALLOC(sizeof(schema_rule_t));
  if (!rule) {
    LOG_ERROR("Out of memory", field_path);
    return NULL;
  }

  rule->type = type;
  rule->field_path = BUFFER_STRDUP(field_path);
  rule->next = NULL;
  rule->regex_compiled = 0;

  /* Initialize params based on rule type */
  switch (type) {
    case SCHEMA_TYPE_CHECK:
      rule->params.type_value = JSON_OBJECT; /* Default to object */
      TRACE_DB("Initialized type check rule with default JSON_OBJECT.");
      break;

    case SCHEMA_REQUIRED:
      rule->params.required = 1; /* Default to required = true */
      TRACE_DB("Initialized required rule with default value: true.");
      break;

    case SCHEMA_MIN_LENGTH:
      rule->params.min_length = 0; /* Default to min length = 0 */
      TRACE_DB("Initialized minLength rule with default value: 0.");
      break;

    case SCHEMA_MAX_LENGTH:
      rule->params.max_length = 1024; /* Default to max length = 1024 */
      TRACE_DB("Initialized maxLength rule with default value: 1024.");
      break;

    case SCHEMA_MIN_VALUE:
      rule->params.min_value = 0.0; /* Default to min value = 0 */
      TRACE_DB("Initialized minValue rule with default value: 0.0.");
      break;

    case SCHEMA_MAX_VALUE:
      rule->params.max_value = 1000000.0; /* Default to max value = 1000000 */
      TRACE_DB("Initialized maxValue rule with default value: 1000000.0.");
      break;

    case SCHEMA_PATTERN:
      rule->params.pattern = NULL; /* Will be set later */
      TRACE_DB("Initialized pattern rule with NULL pattern (to be set later).");
      break;

    case SCHEMA_ENUM:
      rule->params.enum_values = NULL; /* Will be set later */
      TRACE_DB("Initialized enum rule with NULL values (to be set later).");
      break;

    case SCHEMA_NESTED:
      rule->params.nested = NULL; /* Will be set later */
      TRACE_DB("Initialized nested rule with NULL schema (to be set later).");
      break;
  }

  LOG_INFO("Schema rule for field '%s' of type '%s' created.", field_path, type_str);
  return rule;
}

/* Free schema rule and associated resources */
void db_free_schema_rule(schema_rule_t* rule) {
  if (!rule) {
    TRACE_DB("Attempt to free NULL schema rule, ignoring.");
    return;
  }

  const char* type_str = "unknown";
  switch (rule->type) {
    case SCHEMA_TYPE_CHECK: type_str = "type"; break;
    case SCHEMA_REQUIRED: type_str = "required"; break;
    case SCHEMA_MIN_LENGTH: type_str = "minLength"; break;
    case SCHEMA_MAX_LENGTH: type_str = "maxLength"; break;
    case SCHEMA_MIN_VALUE: type_str = "minValue"; break;
    case SCHEMA_MAX_VALUE: type_str = "maxValue"; break;
    case SCHEMA_PATTERN: type_str = "pattern"; break;
    case SCHEMA_ENUM: type_str = "enum"; break;
    case SCHEMA_NESTED: type_str = "nested"; break;
  }

  TRACE_DB("Freeing schema rule of type '%s' for field path: %s",
       type_str, rule->field_path ? rule->field_path : "NULL");

  /* Free field path */
  if (rule->field_path) {
    TRACE_DB("Freeing field path: %s", rule->field_path);
    BUFFER_FREE(rule->field_path);
  }

  /* Free regex pattern if compiled */
  if (rule->regex_compiled) {
    TRACE_DB("Freeing compiled regex pattern.");
    regfree(&rule->regex);
  }

  /* Free resources based on rule type */
  switch (rule->type) {
    case SCHEMA_PATTERN:
      if (rule->params.pattern) {
        TRACE_DB("Freeing pattern: %s", rule->params.pattern);
        BUFFER_FREE(rule->params.pattern);
      }
      break;

    case SCHEMA_ENUM:
      TRACE_DB("Note: Enum values are freed separately as they are JSON values.");
      /* Enum values are freed separately as they are JSON values */
      break;

    case SCHEMA_NESTED:
      if (rule->params.nested) {
        TRACE_DB("Freeing nested schema.");
        db_free_schema(rule->params.nested);
      }
      break;

    default:
      /* No additional resources to free */
      TRACE_DB("No additional resources to free for rule type '%s'", type_str);
      break;
  }

  /* Free rule structure */
  TRACE_DB("Freeing rule structure.");
  BUFFER_FREE(rule);

  LOG_DEBUG("Schema rule freed successfully.");
}

/* Add a rule to a schema */
int db_add_schema_rule(schema_t* schema, schema_rule_t* rule) {
  if (!schema || !rule) {
    LOG_ERROR("Cannot add schema rule: NULL schema or rule.");
    return 0;
  }

  LOG_DEBUG("Adding rule for field '%s' to schema '%s'",
       rule->field_path ? rule->field_path : "NULL",
       schema->name ? schema->name : "unnamed");

  /* Compile regex if it's a pattern rule */
  if (rule->type == SCHEMA_PATTERN && rule->params.pattern) {
    LOG_DEBUG("Compiling regex pattern: %s", rule->params.pattern);
    if (regcomp(&rule->regex, rule->params.pattern, REG_EXTENDED)) {
      LOG_ERROR("compile regex pattern: %s", rule->params.pattern);
      return 0; /* Regex compilation failed */
    }
    rule->regex_compiled = 1;
    LOG_DEBUG("Regex pattern compiled successfully.");
  }

  /* Add rule to the end of the rules list */
  if (!schema->rules) {
    TRACE_DB("Adding as first rule in schema.");
    schema->rules = rule;
  } else {
    TRACE_DB("Adding to end of rules list.");
    schema_rule_t* current = schema->rules;
    while (current->next) {
      current = current->next;
    }
    current->next = rule;
  }

  LOG_INFO("Rule for field '%s' added to schema '%s'",
      rule->field_path, schema->name ? schema->name : "unnamed");
  return 1;
}

/* Get a JSON value at a specific path (dot notation) */
static json_value_t* get_json_value_at_path(json_value_t* root, const char* path) {
  TRACE_DB("Getting JSON value at path: %s", path ? path : "NULL");

  if (!root || !path) {
    TRACE_DB("NULL root or path, returning NULL.");
    return NULL;
  }

  /* Create a copy of the path for tokenization */
  char* path_copy = BUFFER_STRDUP(path);
  if (!path_copy) {
    LOG_ERROR("Cannot allocate memory for path copy.");
    return NULL;
  }

  /* Tokenize the path */
  char* token = strtok(path_copy, ".");
  json_value_t* current = root;

  while (token && current) {
    TRACE_DB("Processing path segment: %s", token);

    if (current->type == JSON_OBJECT) {
      TRACE_DB("Current is an object, getting property: %s", token);
      current = json_object_get(current, token);
    } else if (current->type == JSON_ARRAY) {
      TRACE_DB("Current is an array, trying to parse index: %s", token);
      /* Check if token is a numeric index */
      char* endptr;
      long index = strtol(token, &endptr, 10);
      if (*endptr == '\0' && index >= 0) {
        TRACE_DB("Getting array element at index: %ld", index);
        current = json_array_get(current, index);
      } else {
        LOG_DEBUG("Invalid array index: %s.", token);
        current = NULL;
      }
    } else {
      LOG_DEBUG("Cannot navigate deeper from non-container type.");
      current = NULL;
    }

    token = strtok(NULL, ".");
  }

  BUFFER_FREE(path_copy);

  if (current) {
    const char* type_str = "unknown";
    switch (current->type) {
      case JSON_NULL: type_str = "null"; break;
      case JSON_BOOLEAN: type_str = "boolean"; break;
      case JSON_INTEGER: type_str = "integer"; break;
      case JSON_NUMBER: type_str = "number"; break;
      case JSON_STRING: type_str = "string"; break;
      case JSON_ARRAY: type_str = "array"; break;
      case JSON_OBJECT: type_str = "object"; break;
    }
    TRACE_DB("Found value at path '%s' of type %s", path, type_str);
  } else {
    TRACE_DB("No value found at path: %s", path);
  }

  return current;
}

/* Validate a document against a schema */
schema_validation_result_t db_validate_document(schema_t* schema, json_value_t* document) {
  schema_validation_result_t result = {1, NULL, NULL}; /* Default to valid */

  LOG_DEBUG("Validating document against schema '%s'",
       schema ? (schema->name ? schema->name : "unnamed") : "NULL");

  if (!schema || !document) {
    LOG_ERROR("Invalid schema or document: schema=%p, document=%p",
         (void*)schema, (void*)document);
    result.is_valid = 0;
    result.error_message = BUFFER_STRDUP("Invalid schema or document");
    return result;
  }

  /* Count rules for logging */
  int rule_count = 0;
  schema_rule_t* counting = schema->rules;
  while (counting) {
    rule_count++;
    counting = counting->next;
  }
  LOG_DEBUG("Validating against %d rules", rule_count);

  /* Check each rule */
  schema_rule_t* rule = schema->rules;
  int rule_index = 0;
  while (rule) {
    rule_index++;

    const char* type_str = "unknown";
    switch (rule->type) {
      case SCHEMA_TYPE_CHECK: type_str = "type"; break;
      case SCHEMA_REQUIRED: type_str = "required"; break;
      case SCHEMA_MIN_LENGTH: type_str = "minLength"; break;
      case SCHEMA_MAX_LENGTH: type_str = "maxLength"; break;
      case SCHEMA_MIN_VALUE: type_str = "minValue"; break;
      case SCHEMA_MAX_VALUE: type_str = "maxValue"; break;
      case SCHEMA_PATTERN: type_str = "pattern"; break;
      case SCHEMA_ENUM: type_str = "enum"; break;
      case SCHEMA_NESTED: type_str = "nested"; break;
    }

    TRACE_DB("Checking rule %d/%d: type='%s', field='%s'",
         rule_index, rule_count, type_str, rule->field_path);

    json_value_t* value = get_json_value_at_path(document, rule->field_path);

    switch (rule->type) {
      case SCHEMA_TYPE_CHECK:
        if (value && value->type != (json_type_t)rule->params.type_value) {
          LOG_INFO("Type mismatch for field '%s': expected=%d, actual=%d",
              rule->field_path, rule->params.type_value, (int)value->type);
          result.is_valid = 0;
          result.error_field = BUFFER_STRDUP(rule->field_path);
          result.error_message = BUFFER_STRDUP("Type mismatch");
          return result;
        }
        break;

      case SCHEMA_REQUIRED:
        if (rule->params.required && !value) {
          LOG_INFO("Required field '%s' is missing", rule->field_path);
          result.is_valid = 0;
          result.error_field = BUFFER_STRDUP(rule->field_path);
          result.error_message = BUFFER_STRDUP("Required field missing");
          return result;
        }
        break;

      case SCHEMA_MIN_LENGTH:
        if (value && value->type == JSON_STRING &&
          strlen(value->value.string) < (size_t)rule->params.min_length) {
          LOG_INFO("String too short for field '%s': min=%d, actual=%zu",
              rule->field_path, rule->params.min_length,
              strlen(value->value.string));
          result.is_valid = 0;
          result.error_field = BUFFER_STRDUP(rule->field_path);
          result.error_message = BUFFER_STRDUP("String too short");
          return result;
        }
        break;

      case SCHEMA_MAX_LENGTH:
        if (value && value->type == JSON_STRING &&
          strlen(value->value.string) > (size_t)rule->params.max_length) {
          LOG_INFO("String too long for field '%s': max=%d, actual=%zu",
              rule->field_path, rule->params.max_length,
              strlen(value->value.string));
          result.is_valid = 0;
          result.error_field = BUFFER_STRDUP(rule->field_path);
          result.error_message = BUFFER_STRDUP("String too long");
          return result;
        }
        break;

      case SCHEMA_MIN_VALUE:
        if (value) {
          if (value->type == JSON_NUMBER && value->value.number < rule->params.min_value) {
            LOG_INFO("Number too small for field '%s': min=%.2f, actual=%.2f",
                rule->field_path, rule->params.min_value, value->value.number);
            result.is_valid = 0;
            result.error_field = BUFFER_STRDUP(rule->field_path);
            result.error_message = BUFFER_STRDUP("Value too small");
            return result;
          } else if (value->type == JSON_INTEGER && (double)value->value.integer < rule->params.min_value) {
            LOG_INFO("Integer too small for field '%s': min=%.2f, actual=%ld",
                rule->field_path, rule->params.min_value, value->value.integer);
            result.is_valid = 0;
            result.error_field = BUFFER_STRDUP(rule->field_path);
            result.error_message = BUFFER_STRDUP("Value too small");
            return result;
          }
        }
        break;

      case SCHEMA_MAX_VALUE:
        if (value) {
          if (value->type == JSON_NUMBER && value->value.number > rule->params.max_value) {
            LOG_INFO("Number too large for field '%s': max=%.2f, actual=%.2f",
                rule->field_path, rule->params.max_value, value->value.number);
            result.is_valid = 0;
            result.error_field = BUFFER_STRDUP(rule->field_path);
            result.error_message = BUFFER_STRDUP("Value too large");
            return result;
          } else if (value->type == JSON_INTEGER && (double)value->value.integer > rule->params.max_value) {
            LOG_INFO("Integer too large for field '%s': max=%.2f, actual=%ld",
                rule->field_path, rule->params.max_value, value->value.integer);
            result.is_valid = 0;
            result.error_field = BUFFER_STRDUP(rule->field_path);
            result.error_message = BUFFER_STRDUP("Value too large");
            return result;
          }
        }
        break;

      case SCHEMA_PATTERN:
        if (value && value->type == JSON_STRING && rule->regex_compiled) {
          TRACE_DB("Checking pattern for field '%s': pattern='%s', value='%s'",
               rule->field_path, rule->params.pattern, value->value.string);
          if (regexec(&rule->regex, value->value.string, 0, NULL, 0) != 0) {
            LOG_INFO("Pattern match failed for field '%s'", rule->field_path);
            result.is_valid = 0;
            result.error_field = BUFFER_STRDUP(rule->field_path);
            result.error_message = BUFFER_STRDUP("Pattern match failed");
            return result;
          }
        }
        break;

      case SCHEMA_ENUM:
        if (value && rule->params.enum_values) {
          TRACE_DB("Checking enum values for field '%s'", rule->field_path);
          int found = 0;
          json_value_t* enum_array = rule->params.enum_values;

          if (enum_array->type == JSON_ARRAY) {
            size_t enum_size = json_array_size(enum_array);
            TRACE_DB("Enum contains %zu values", enum_size);

            for (size_t i = 0; i < enum_size; i++) {
              json_value_t* enum_value = json_array_get(enum_array, i);
              if (json_equals(value, enum_value)) {
                TRACE_DB("Value matches enum at index %zu", i);
                found = 1;
                break;
              }
            }

            if (!found) {
              LOG_INFO("Value not in enum for field '%s'.", rule->field_path);
              result.is_valid = 0;
              result.error_field = BUFFER_STRDUP(rule->field_path);
              result.error_message = BUFFER_STRDUP("Value not in enum");
              return result;
            }
          }
        }
        break;

      case SCHEMA_NESTED:
        if (value && rule->params.nested) {
          LOG_DEBUG("Validating nested schema for field '%s'", rule->field_path);
          schema_validation_result_t nested_result = db_validate_document(rule->params.nested, value);
          if (!nested_result.is_valid) {
            LOG_INFO("Nested validation failed for field '%s': %s",
                rule->field_path, nested_result.error_message);

            char* nested_field = NULL;
            if (nested_result.error_field) {
              size_t len = strlen(rule->field_path) + 1 + strlen(nested_result.error_field) + 1;
              nested_field = (char*)BUFFER_ALLOC(len);
              if (nested_field) {
                sprintf(nested_field, "%s.%s", rule->field_path, nested_result.error_field);
                TRACE_DB("Full error path: %s", nested_field);
              }
              BUFFER_FREE(nested_result.error_field);
            }

            result.is_valid = 0;
            result.error_field = nested_field ? nested_field : BUFFER_STRDUP(rule->field_path);
            result.error_message = nested_result.error_message;
            return result;
          }
        }
        break;
    }

    rule = rule->next;
  }

  LOG_INFO("Document validation successful for schema '%s'",
      schema->name ? schema->name : "unnamed");
  return result;
}

/* Attach a schema to a collection */
int db_attach_schema(database_t* db, const char* collection, schema_t* schema) {
  LOG_DEBUG("Attaching schema '%s' to collection '%s'",
       schema ? (schema->name ? schema->name : "unnamed") : "NULL",
       collection ? collection : "NULL");

  if (!db || !collection || !schema) {
    LOG_ERROR("attach schema: invalid parameters (db=%p, collection=%s, schema=%p)",
         (void*)db, collection ? collection : "NULL", (void*)schema);
    return 0;
  }

  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Cannot attach schema: collection '%s' not found.", collection);
    return 0;
  }

  /* Lock the collection */
  TRACE_DB("Locking collection '%s'", collection);
  pthread_mutex_lock(&coll->lock);

  /* Free existing schema if any */
  if (coll->schema) {
    LOG_INFO("Replacing existing schema '%s' for collection '%s'",
        coll->schema->name ? coll->schema->name : "unnamed", collection);
    db_free_schema(coll->schema);
  } else {
    LOG_INFO("Adding new schema to collection '%s'", collection);
  }

  /* Attach the new schema */
  coll->schema = schema;

  /* Unlock the collection */
  TRACE_DB("Unlocking collection '%s'", collection);
  pthread_mutex_unlock(&coll->lock);

  /* Mark database as modified */
  TRACE_DB("Marking database as modified.");
  pthread_rwlock_wrlock(&db->rwlock);
  db->is_modified = 1;
  pthread_rwlock_unlock(&db->rwlock);

  LOG_INFO("Schema '%s' attached to collection '%s'",
      schema->name ? schema->name : "unnamed", collection);
  return 1;
}

/* Detach a schema from a collection */
int db_detach_schema(database_t* db, const char* collection) {
  LOG_DEBUG("Detaching schema from collection '%s'", collection ? collection : "NULL");

  if (!db || !collection) {
    LOG_ERROR("detach schema: invalid parameters (db=%p, collection=%s)",
         (void*)db, collection ? collection : "NULL");
    return 0;
  }

  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Cannot detach schema: collection '%s' not found.", collection);
    return 0;
  }

  /* Lock the collection */
  TRACE_DB("Locking collection '%s'", collection);
  pthread_mutex_lock(&coll->lock);

  /* Free existing schema if any */
  if (coll->schema) {
    LOG_INFO("Detaching schema '%s' from collection '%s'",
        coll->schema->name ? coll->schema->name : "unnamed", collection);
    db_free_schema(coll->schema);
    coll->schema = NULL;
  } else {
    LOG_INFO("No schema attached to collection '%s', nothing to detach", collection);
  }

  /* Unlock the collection */
  TRACE_DB("Unlocking collection '%s'", collection);
  pthread_mutex_unlock(&coll->lock);

  /* Mark database as modified */
  TRACE_DB("Marking database as modified.");
  pthread_rwlock_wrlock(&db->rwlock);
  db->is_modified = 1;
  pthread_rwlock_unlock(&db->rwlock);

  LOG_INFO("Schema detached from collection '%s'", collection);
  return 1;
}

/* Get the schema for a collection */
schema_t* db_get_schema(database_t* db, const char* collection) {
  LOG_DEBUG("Getting schema for collection '%s'", collection ? collection : "NULL");

  if (!db || !collection) {
    LOG_ERROR("get schema: invalid parameters (db=%p, collection=%s)",
         (void*)db, collection ? collection : "NULL");
    return NULL;
  }

  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("Cannot get schema: collection '%s' not found.", collection);
    return NULL;
  }

  /* Lock the collection */
  TRACE_DB("Locking collection '%s'", collection);
  pthread_mutex_lock(&coll->lock);

  /* Get the schema */
  schema_t* schema = coll->schema;

  /* Unlock the collection */
  TRACE_DB("Unlocking collection '%s'", collection);
  pthread_mutex_unlock(&coll->lock);

  if (schema) {
    LOG_INFO("Found schema '%s' for collection '%s'",
        schema->name ? schema->name : "unnamed", collection);
  } else {
    LOG_INFO("No schema found for collection '%s'", collection);
  }

  return schema;
}

/* Convert schema to JSON */
json_value_t* db_schema_to_json(schema_t* schema) {
  LOG_DEBUG("Converting schema '%s' to JSON",
       schema ? (schema->name ? schema->name : "unnamed") : "NULL");

  if (!schema) {
    LOG_ERROR("Cannot convert schema to JSON: NULL schema.");
    return NULL;
  }

  /* Create schema object */
  json_value_t* json = json_create_object();
  if (!json) {
    LOG_ERROR("Cannot create JSON object for schema.");
    return NULL;
  }

  /* Add basic schema properties */
  TRACE_DB("Adding schema name: %s", schema->name);
  json_object_set(json, "name", json_create_string(schema->name));

  if (schema->description) {
    TRACE_DB("Adding schema description.");
    json_object_set(json, "description", json_create_string(schema->description));
  }

  /* Create rules array */
  TRACE_DB("Creating rules array.");
  json_value_t* rules_array = json_create_array();
  if (!rules_array) {
    LOG_ERROR("Cannot create rules array.");
    /* CHECKPOINT: json_free(json); */
    return NULL;
  }

  /* Count rules for logging */
  int rule_count = 0;
  schema_rule_t* counting = schema->rules;
  while (counting) {
    rule_count++;
    counting = counting->next;
  }
  LOG_DEBUG("Converting %d rules to JSON", rule_count);

  /* Add each rule to the array */
  schema_rule_t* rule = schema->rules;
  int rule_index = 0;
  while (rule) {
    rule_index++;
    TRACE_DB("Converting rule %d/%d for field '%s'",
         rule_index, rule_count, rule->field_path);

    json_value_t* rule_obj = json_create_object();
    if (!rule_obj) {
      LOG_ERROR("Cannot create JSON object for rule %d.", rule_index);
      /* CHECKPOINT: json_free(rules_array); */
      /* CHECKPOINT: json_free(json); */
      return NULL;
    }

    /* Add rule properties */
    json_object_set(rule_obj, "field", json_create_string(rule->field_path));

    /* Set rule type */
    const char* type_str = NULL;
    switch (rule->type) {
      case SCHEMA_TYPE_CHECK:
        type_str = "type";
        break;
      case SCHEMA_REQUIRED:
        type_str = "required";
        break;
      case SCHEMA_MIN_LENGTH:
        type_str = "minLength";
        break;
      case SCHEMA_MAX_LENGTH:
        type_str = "maxLength";
        break;
      case SCHEMA_MIN_VALUE:
        type_str = "minValue";
        break;
      case SCHEMA_MAX_VALUE:
        type_str = "maxValue";
        break;
      case SCHEMA_PATTERN:
        type_str = "pattern";
        break;
      case SCHEMA_ENUM:
        type_str = "enum";
        break;
      case SCHEMA_NESTED:
        type_str = "nested";
        break;
    }

    TRACE_DB("Setting rule type: %s", type_str);
    json_object_set(rule_obj, "type", json_create_string(type_str));

    /* Add rule parameters based on type */
    switch (rule->type) {
      case SCHEMA_TYPE_CHECK:
        {
          const char* type_value = NULL;
          switch (rule->params.type_value) {
            case JSON_NULL: type_value = "null"; break;
            case JSON_BOOLEAN: type_value = "boolean"; break;
            case JSON_INTEGER: type_value = "integer"; break;
            case JSON_NUMBER: type_value = "number"; break;
            case JSON_STRING: type_value = "string"; break;
            case JSON_ARRAY: type_value = "array"; break;
            case JSON_OBJECT: type_value = "object"; break;
            default: type_value = "any";
          }
          TRACE_DB("Setting type check value: %s", type_value);
          json_object_set(rule_obj, "value", json_create_string(type_value));
        }
        break;

      case SCHEMA_REQUIRED:
        TRACE_DB("Setting required value: %d", rule->params.required);
        json_object_set(rule_obj, "value", json_create_boolean(rule->params.required));
        break;

      case SCHEMA_MIN_LENGTH:
        TRACE_DB("Setting minLength value: %d", rule->params.min_length);
        json_object_set(rule_obj, "value", json_create_integer(rule->params.min_length));
        break;

      case SCHEMA_MAX_LENGTH:
        TRACE_DB("Setting maxLength value: %d", rule->params.max_length);
        json_object_set(rule_obj, "value", json_create_integer(rule->params.max_length));
        break;

      case SCHEMA_MIN_VALUE:
        TRACE_DB("Setting minValue value: %.2f", rule->params.min_value);
        json_object_set(rule_obj, "value", json_create_number(rule->params.min_value));
        break;

      case SCHEMA_MAX_VALUE:
        TRACE_DB("Setting maxValue value: %.2f", rule->params.max_value);
        json_object_set(rule_obj, "value", json_create_number(rule->params.max_value));
        break;

      case SCHEMA_PATTERN:
        if (rule->params.pattern) {
          TRACE_DB("Setting pattern value: %s", rule->params.pattern);
          json_object_set(rule_obj, "value", json_create_string(rule->params.pattern));
        } else {
          TRACE_DB("No pattern value to set.");
        }
        break;

      case SCHEMA_ENUM:
        if (rule->params.enum_values) {
          TRACE_DB("Setting enum values.");
          json_object_set(rule_obj, "value", json_clone(rule->params.enum_values));
        } else {
          TRACE_DB("No enum values to set.");
        }
        break;

      case SCHEMA_NESTED:
        if (rule->params.nested) {
          TRACE_DB("Converting nested schema.");
          json_value_t* nested_json = db_schema_to_json(rule->params.nested);
          if (nested_json) {
            TRACE_DB("Setting nested schema.");
            json_object_set(rule_obj, "value", nested_json);
          } else {
            LOG_WARNING("Cannot convert nested schema to JSON.");
          }
        } else {
          TRACE_DB("No nested schema to convert.");
        }
        break;
    }

    /* Add rule to rules array */
    TRACE_DB("Adding rule to rules array.");
    json_array_append(rules_array, rule_obj);

    rule = rule->next;
  }

  /* Add rules array to schema object */
  TRACE_DB("Adding rules array to schema object.");
  json_object_set(json, "rules", rules_array);

  LOG_INFO("Schema '%s' converted to JSON", schema->name);
  return json;
}

/* Create schema from JSON */
schema_t* db_schema_from_json(json_value_t* json) {
  LOG_DEBUG("Creating schema from JSON.");

  if (!json || json->type != JSON_OBJECT) {
    LOG_ERROR("Cannot create schema from JSON: invalid JSON or not an object.");
    return NULL;
  }

  /* Get schema name and description */
  json_value_t* name_val = json_object_get(json, "name");
  if (!name_val || name_val->type != JSON_STRING) {
    LOG_ERROR("Cannot create schema from JSON: missing or invalid 'name' property.");
    return NULL;
  }

  TRACE_DB("Found schema name: %s", name_val->value.string);

  json_value_t* desc_val = json_object_get(json, "description");
  const char* description = NULL;
  if (desc_val && desc_val->type == JSON_STRING) {
    description = desc_val->value.string;
    TRACE_DB("Found schema description: %s", description);
  } else {
    TRACE_DB("No description found in schema JSON.");
  }

  /* Create schema */
  LOG_DEBUG("Creating schema with name '%s'", name_val->value.string);
  schema_t* schema = db_create_schema(name_val->value.string, description);
  if (!schema) {
    LOG_ERROR("Cannot create schema object.");
    return NULL;
  }

  /* Get rules array */
  json_value_t* rules_arr = json_object_get(json, "rules");
  if (!rules_arr || rules_arr->type != JSON_ARRAY) {
    LOG_ERROR("Cannot get rules array from schema JSON.");
    db_free_schema(schema);
    return NULL;
  }

  /* Process each rule */
  size_t rule_count = json_array_size(rules_arr);
  LOG_DEBUG("Processing %zu rules from JSON", rule_count);

  for (size_t i = 0; i < rule_count; i++) {
    TRACE_DB("Processing rule %zu/%zu", i + 1, rule_count);

    json_value_t* rule_obj = json_array_get(rules_arr, i);
    if (!rule_obj || rule_obj->type != JSON_OBJECT) {
      LOG_WARNING("Skipping rule %zu: not a JSON object", i + 1);
      continue;
    }

    /* Get rule field and type */
    json_value_t* field_val = json_object_get(rule_obj, "field");
    json_value_t* type_val = json_object_get(rule_obj, "type");
    json_value_t* value_val = json_object_get(rule_obj, "value");

    if (!field_val || field_val->type != JSON_STRING ||
      !type_val || type_val->type != JSON_STRING) {
      LOG_WARNING("Skipping rule %zu: missing or invalid 'field' or 'type' property", i + 1);
      continue;
    }

    TRACE_DB("Rule %zu: field='%s', type='%s'",
         i + 1, field_val->value.string, type_val->value.string);

    /* Determine rule type */
    schema_rule_type_t rule_type;
    if (strcmp(type_val->value.string, "type") == 0) {
      rule_type = SCHEMA_TYPE_CHECK;
    } else if (strcmp(type_val->value.string, "required") == 0) {
      rule_type = SCHEMA_REQUIRED;
    } else if (strcmp(type_val->value.string, "minLength") == 0) {
      rule_type = SCHEMA_MIN_LENGTH;
    } else if (strcmp(type_val->value.string, "maxLength") == 0) {
      rule_type = SCHEMA_MAX_LENGTH;
    } else if (strcmp(type_val->value.string, "minValue") == 0) {
      rule_type = SCHEMA_MIN_VALUE;
    } else if (strcmp(type_val->value.string, "maxValue") == 0) {
      rule_type = SCHEMA_MAX_VALUE;
    } else if (strcmp(type_val->value.string, "pattern") == 0) {
      rule_type = SCHEMA_PATTERN;
    } else if (strcmp(type_val->value.string, "enum") == 0) {
      rule_type = SCHEMA_ENUM;
    } else if (strcmp(type_val->value.string, "nested") == 0) {
      rule_type = SCHEMA_NESTED;
    } else {
      LOG_WARNING("Skipping rule %zu: unknown type '%s'", i + 1, type_val->value.string);
      continue; /* Unknown rule type */
    }

    /* Create rule */
    TRACE_DB("Creating rule of type '%s' for field '%s'",
         type_val->value.string, field_val->value.string);
    schema_rule_t* rule = db_create_schema_rule(rule_type, field_val->value.string);
    if (!rule) {
      LOG_ERROR("Cannot create rule of type '%s' for field '%s'.",
           type_val->value.string, field_val->value.string);
      continue;
    }

    /* Set rule parameters based on type */
    if (value_val) {
      TRACE_DB("Processing rule value.");

      switch (rule_type) {
        case SCHEMA_TYPE_CHECK:
          if (value_val->type == JSON_STRING) {
            TRACE_DB("Processing type check value: %s", value_val->value.string);
            if (strcmp(value_val->value.string, "null") == 0) {
              rule->params.type_value = JSON_NULL;
            } else if (strcmp(value_val->value.string, "boolean") == 0) {
              rule->params.type_value = JSON_BOOLEAN;
            } else if (strcmp(value_val->value.string, "integer") == 0) {
              rule->params.type_value = JSON_INTEGER;
            } else if (strcmp(value_val->value.string, "number") == 0) {
              rule->params.type_value = JSON_NUMBER;
            } else if (strcmp(value_val->value.string, "string") == 0) {
              rule->params.type_value = JSON_STRING;
            } else if (strcmp(value_val->value.string, "array") == 0) {
              rule->params.type_value = JSON_ARRAY;
            } else if (strcmp(value_val->value.string, "object") == 0) {
              rule->params.type_value = JSON_OBJECT;
            }
            TRACE_DB("Set type check value to: %d", rule->params.type_value);
          }
          break;

        case SCHEMA_REQUIRED:
          if (value_val->type == JSON_BOOLEAN) {
            rule->params.required = value_val->value.boolean;
            TRACE_DB("Set required value to: %d", rule->params.required);
          }
          break;

        case SCHEMA_MIN_LENGTH:
          if (value_val->type == JSON_INTEGER) {
            rule->params.min_length = value_val->value.integer;
            TRACE_DB("Set minLength value to: %ld", value_val->value.integer);
          } else if (value_val->type == JSON_NUMBER) {
            rule->params.min_length = (int)value_val->value.number;
            TRACE_DB("Set minLength value to: %d (from %f)",
                 rule->params.min_length, value_val->value.number);
          }
          break;

        case SCHEMA_MAX_LENGTH:
          if (value_val->type == JSON_INTEGER) {
            rule->params.max_length = value_val->value.integer;
            TRACE_DB("Set maxLength value to: %ld", value_val->value.integer);
          } else if (value_val->type == JSON_NUMBER) {
            rule->params.max_length = (int)value_val->value.number;
            TRACE_DB("Set maxLength value to: %d (from %f)",
                 rule->params.max_length, value_val->value.number);
          }
          break;

        case SCHEMA_MIN_VALUE:
          if (value_val->type == JSON_INTEGER) {
            rule->params.min_value = (double)value_val->value.integer;
            TRACE_DB("Set minValue value to: %.2f (from %ld)",
                 rule->params.min_value, value_val->value.integer);
          } else if (value_val->type == JSON_NUMBER) {
            rule->params.min_value = value_val->value.number;
            TRACE_DB("Set minValue value to: %.2f", rule->params.min_value);
          }
          break;

        case SCHEMA_MAX_VALUE:
          if (value_val->type == JSON_INTEGER) {
            rule->params.max_value = (double)value_val->value.integer;
            TRACE_DB("Set maxValue value to: %.2f (from %ld)",
                 rule->params.max_value, value_val->value.integer);
          } else if (value_val->type == JSON_NUMBER) {
            rule->params.max_value = value_val->value.number;
            TRACE_DB("Set maxValue value to: %.2f", rule->params.max_value);
          }
          break;

        case SCHEMA_PATTERN:
          if (value_val->type == JSON_STRING) {
            rule->params.pattern = BUFFER_STRDUP(value_val->value.string);
            TRACE_DB("Set pattern value to: %s", rule->params.pattern);
          }
          break;

        case SCHEMA_ENUM:
          if (value_val->type == JSON_ARRAY) {
            rule->params.enum_values = json_clone(value_val);
            TRACE_DB("Set enum values (array of %zu elements)",
                 json_array_size(value_val));
          }
          break;

        case SCHEMA_NESTED:
          if (value_val->type == JSON_OBJECT) {
            TRACE_DB("Processing nested schema.");
            rule->params.nested = db_schema_from_json(value_val);
            if (rule->params.nested) {
              TRACE_DB("Set nested schema: %s",
                   rule->params.nested->name ? rule->params.nested->name : "unnamed");
            } else {
              LOG_WARNING("Cannot create nested schema.");
            }
          }
          break;
      }
    } else {
      LOG_WARNING("Rule has no value property, using defaults.");
    }

    /* Add rule to schema */
    TRACE_DB("Adding rule to schema.");
    if (!db_add_schema_rule(schema, rule)) {
    LOG_ERROR("Cannot add rule to schema.");
      db_free_schema_rule(rule);
    } else {
      TRACE_DB("Rule added successfully.");
    }
  }

  LOG_INFO("Schema '%s' created from JSON with %zu rules",
      schema->name, rule_count);
  return schema;
}

/* Validate all documents in a collection against its schema */
int db_validate_collection(database_t* db, const char* collection) {
  LOG_DEBUG("Validating all documents in collection '%s' against schema",
       collection ? collection : "NULL");

  if (!db || !collection) {
    LOG_ERROR("validate collection: invalid parameters (db=%p, collection=%s)",
         (void*)db, collection ? collection : "NULL");
    return 0;
  }

  db_collection_t* coll = db_get_collection(db, collection);
  if (!coll) {
    LOG_ERROR("validate collection: collection '%s' not found", collection);
    return 0;
  }

  if (!coll->schema) {
    LOG_WARNING("Collection '%s' has no schema attached, nothing to validate", collection);
    return 0; /* No schema */
  }

  LOG_INFO("Validating collection '%s' against schema '%s'",
      collection, coll->schema->name ? coll->schema->name : "unnamed");

  /* Lock the collection */
  TRACE_DB("Locking collection '%s'", collection);
  pthread_mutex_lock(&coll->lock);

  /* Validate each document */
  int valid = 1;
  if (coll->documents && coll->documents->type == JSON_ARRAY) {
    size_t doc_count = json_array_size(coll->documents);
    LOG_DEBUG("Validating %zu documents", doc_count);

    for (size_t i = 0; i < doc_count; i++) {
      TRACE_DB("Validating document %zu/%zu", i + 1, doc_count);

      json_value_t* doc = json_array_get(coll->documents, i);
      if (doc) {
        /* Get document ID for better logging */
        const char* doc_id = "unknown";
        json_value_t* id_val = json_object_get(doc, "uuid");
        if (id_val && id_val->type == JSON_STRING) {
          doc_id = id_val->value.string;
        }

        TRACE_DB("Validating document ID: %s", doc_id);
        schema_validation_result_t result = db_validate_document(coll->schema, doc);

        if (!result.is_valid) {
          LOG_WARNING("Document '%s' failed validation: %s (field: %s)",
               doc_id,
               result.error_message ? result.error_message : "unknown error",
               result.error_field ? result.error_field : "unknown field");

          valid = 0;

          /* Free validation result resources */
          if (result.error_field) BUFFER_FREE(result.error_field);
          if (result.error_message) BUFFER_FREE(result.error_message);
          break;
        } else {
          TRACE_DB("Document '%s' validated successfully", doc_id);
        }
      } else {
        LOG_WARNING("Null document at index %zu", i);
      }
    }
  } else {
    LOG_WARNING("Collection '%s' has no documents array or invalid type", collection);
  }

  /* Unlock the collection */
  TRACE_DB("Unlocking collection '%s'", collection);
  pthread_mutex_unlock(&coll->lock);

  if (valid) {
    LOG_INFO("All documents in collection '%s' validated", collection);
  } else {
    LOG_ERROR("Validation failed for collection '%s'.", collection);
  }

  return valid;
}