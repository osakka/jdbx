#include "utils/import_export.h"
#include "database/database.h"
#include "utils/json.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

/* Get the current timestamp as a string */
static char* get_timestamp() {
  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  
  char* timestamp = BUFFER_ALLOC(20);
  if (!timestamp) {
    return NULL;
  }
  
  strftime(timestamp, 20, "%Y%m%d_%H%M%S", tm_info);
  return timestamp;
}

/* Ensure the directory exists */
static int ensure_directory(const char* path) {
  struct stat st = {0};
  
  if (stat(path, &st) == -1) {
    /* Directory doesn't exist, create it */
    if (mkdir(path, 0755) != 0) {
      return 0;
    }
  }
  
  return 1;
}

/* Export the database to a file */
int export_database(database_t* db, const char* output_path) {
  if (!db || !output_path) {
    return EXPORT_ERROR_INVALID_ARGS;
  }
  
  /* Create export directory if needed */
  char* dir_path = BUFFER_STRDUP(output_path);
  if (!dir_path) {
    return EXPORT_ERROR_MEMORY;
  }
  
  /* Extract directory path */
  char* last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0';
    if (!ensure_directory(dir_path)) {
      BUFFER_FREE(dir_path);
      return EXPORT_ERROR_DIRECTORY;
    }
  }
  
  BUFFER_FREE(dir_path);
  
  /* Force database save to ensure latest data is included */
  if (!db_save(db)) {
    return EXPORT_ERROR_DATABASE_SAVE;
  }
  
  /* Export database file */
  FILE* src_file = fopen(db->path, "rb");
  if (!src_file) {
    return EXPORT_ERROR_SOURCE_FILE;
  }
  
  FILE* dst_file = fopen(output_path, "wb");
  if (!dst_file) {
    fclose(src_file);
    return EXPORT_ERROR_DESTINATION_FILE;
  }
  
  /* Copy file contents */
  char buffer[4096];
  size_t bytes_read;
  
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
    if (fwrite(buffer, 1, bytes_read, dst_file) != bytes_read) {
      fclose(src_file);
      fclose(dst_file);
      return EXPORT_ERROR_WRITE;
    }
  }
  
  fclose(src_file);
  fclose(dst_file);
  
  return EXPORT_SUCCESS;
}

/* Export the database with collections filter */
int export_collections(database_t* db, const char* output_path, const char** collections, int num_collections) {
  if (!db || !output_path || !collections || num_collections <= 0) {
    return EXPORT_ERROR_INVALID_ARGS;
  }
  
  /* Create export directory if needed */
  char* dir_path = BUFFER_STRDUP(output_path);
  if (!dir_path) {
    return EXPORT_ERROR_MEMORY;
  }
  
  /* Extract directory path */
  char* last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0';
    if (!ensure_directory(dir_path)) {
      BUFFER_FREE(dir_path);
      return EXPORT_ERROR_DIRECTORY;
    }
  }
  
  BUFFER_FREE(dir_path);
  
  /* Create a new object with only the specified collections */
  json_value_t* export_obj = json_create_object();
  if (!export_obj) {
    return EXPORT_ERROR_MEMORY;
  }
  
  /* Lock database */
  pthread_rwlock_rdlock(&db->rwlock);
  
  /* Copy specified collections */
  for (int i = 0; i < num_collections; i++) {
    const char* coll_name = collections[i];
    
    /* Check if collection exists */
    json_value_t* collection = json_object_get(db->collections, coll_name);
    if (collection && collection->type == JSON_ARRAY) {
      /* Create deep copy of collection */
      char* coll_str = json_stringify(collection);
      if (coll_str) {
        json_value_t* coll_copy = json_parse(coll_str);
        BUFFER_FREE(coll_str);
        
        if (coll_copy) {
          json_object_set(export_obj, coll_name, coll_copy);
        }
      }
    }
  }
  
  pthread_rwlock_unlock(&db->rwlock);
  
  /* Write to file */
  char* json_str = json_stringify(export_obj);
  /* CHECKPOINT: json_free(export_obj); */
  
  if (!json_str) {
    return EXPORT_ERROR_MEMORY;
  }
  
  FILE* file = fopen(output_path, "w");
  if (!file) {
    BUFFER_FREE(json_str);
    return EXPORT_ERROR_DESTINATION_FILE;
  }
  
  int result = EXPORT_SUCCESS;
  if (fputs(json_str, file) == EOF) {
    result = EXPORT_ERROR_WRITE;
  }
  
  fclose(file);
  BUFFER_FREE(json_str);
  
  return result;
}

/* Import a database from a file */
int import_database(database_t* db, const char* input_path, int overwrite) {
  if (!db || !input_path) {
    return IMPORT_ERROR_INVALID_ARGS;
  }
  
  /* Check if input file exists */
  if (access(input_path, F_OK) != 0) {
    return IMPORT_ERROR_SOURCE_FILE;
  }
  
  /* If overwrite is false, only import if database is empty */
  if (!overwrite) {
    pthread_rwlock_rdlock(&db->rwlock);
    size_t num_collections = db->collections->value.object.size;
    pthread_rwlock_unlock(&db->rwlock);
    
    if (num_collections > 0) {
      return IMPORT_ERROR_NOT_EMPTY;
    }
  }
  
  /* Read input file */
  FILE* file = fopen(input_path, "r");
  if (!file) {
    return IMPORT_ERROR_SOURCE_FILE;
  }
  
  /* Get file size */
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  /* Allocate buffer */
  char* buffer = (char*)BUFFER_ALLOC(file_size + 1);
  if (!buffer) {
    fclose(file);
    return IMPORT_ERROR_MEMORY;
  }
  
  /* Read file content */
  size_t read_size = fread(buffer, 1, file_size, file);
  buffer[read_size] = '\0';
  
  fclose(file);
  
  /* Parse JSON */
  json_value_t* import_obj = json_parse(buffer);
  BUFFER_FREE(buffer);
  
  if (!import_obj || import_obj->type != JSON_OBJECT) {
    if (import_obj) {
      /* CHECKPOINT: json_free(import_obj); */
    }
    return IMPORT_ERROR_INVALID_FORMAT;
  }
  
  /* Lock database */
  pthread_rwlock_wrlock(&db->rwlock);
  
  /* If overwrite, clear existing collections */
  if (overwrite) {
    /* CHECKPOINT: json_free(db->collections); */
    db->collections = json_create_object();
    if (!db->collections) {
      pthread_rwlock_unlock(&db->rwlock);
      /* CHECKPOINT: json_free(import_obj); */
      return IMPORT_ERROR_MEMORY;
    }
  }
  
  /* Copy all collections from import object */
  for (size_t i = 0; i < import_obj->value.object.size; i++) {
    const char* coll_name = import_obj->value.object.entries[i].key;
    json_value_t* collection = import_obj->value.object.entries[i].value;
    
    if (collection->type == JSON_ARRAY) {
      /* Create deep copy of collection */
      char* coll_str = json_stringify(collection);
      if (coll_str) {
        json_value_t* coll_copy = json_parse(coll_str);
        BUFFER_FREE(coll_str);
        
        if (coll_copy) {
          json_object_set(db->collections, coll_name, coll_copy);
        }
      }
    }
  }
  
  /* Mark database as modified */
  db->is_modified = 1;
  
  pthread_rwlock_unlock(&db->rwlock);
  
  /* CHECKPOINT: json_free(import_obj); */
  
  /* Save the updated database */
  if (!db_save(db)) {
    return IMPORT_ERROR_DATABASE_SAVE;
  }
  
  return IMPORT_SUCCESS;
}

/* Import specific collections */
int import_collections(database_t* db, const char* input_path, const char** collections, int num_collections, int overwrite) {
  if (!db || !input_path || !collections || num_collections <= 0) {
    return IMPORT_ERROR_INVALID_ARGS;
  }
  
  /* Check if input file exists */
  if (access(input_path, F_OK) != 0) {
    return IMPORT_ERROR_SOURCE_FILE;
  }
  
  /* Read input file */
  FILE* file = fopen(input_path, "r");
  if (!file) {
    return IMPORT_ERROR_SOURCE_FILE;
  }
  
  /* Get file size */
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  /* Allocate buffer */
  char* buffer = (char*)BUFFER_ALLOC(file_size + 1);
  if (!buffer) {
    fclose(file);
    return IMPORT_ERROR_MEMORY;
  }
  
  /* Read file content */
  size_t read_size = fread(buffer, 1, file_size, file);
  buffer[read_size] = '\0';
  
  fclose(file);
  
  /* Parse JSON */
  json_value_t* import_obj = json_parse(buffer);
  BUFFER_FREE(buffer);
  
  if (!import_obj || import_obj->type != JSON_OBJECT) {
    if (import_obj) {
      /* CHECKPOINT: json_free(import_obj); */
    }
    return IMPORT_ERROR_INVALID_FORMAT;
  }
  
  /* Lock database */
  pthread_rwlock_wrlock(&db->rwlock);
  
  /* Import specified collections */
  for (int i = 0; i < num_collections; i++) {
    const char* coll_name = collections[i];
    
    /* Check if collection exists in import object */
    json_value_t* collection = json_object_get(import_obj, coll_name);
    if (collection && collection->type == JSON_ARRAY) {
      /* Check if collection already exists in database */
      if (!overwrite && json_object_has(db->collections, coll_name)) {
        continue;
      }
      
      /* Create deep copy of collection */
      char* coll_str = json_stringify(collection);
      if (coll_str) {
        json_value_t* coll_copy = json_parse(coll_str);
        BUFFER_FREE(coll_str);
        
        if (coll_copy) {
          json_object_set(db->collections, coll_name, coll_copy);
          db->is_modified = 1;
        }
      }
    }
  }
  
  pthread_rwlock_unlock(&db->rwlock);
  
  /* CHECKPOINT: json_free(import_obj); */
  
  /* Save the updated database */
  if (db->is_modified && !db_save(db)) {
    return IMPORT_ERROR_DATABASE_SAVE;
  }
  
  return IMPORT_SUCCESS;
}

/* Create a backup of the database */
char* backup_database(database_t* db, const char* backup_dir) {
  if (!db) {
    return NULL;
  }
  
  /* Set default backup directory if not provided */
  if (!backup_dir) {
    backup_dir = "./backups";
  }
  
  /* Ensure backup directory exists */
  if (!ensure_directory(backup_dir)) {
    return NULL;
  }
  
  /* Create backup filename with timestamp */
  char* timestamp = get_timestamp();
  if (!timestamp) {
    return NULL;
  }
  
  /* Extract original filename from path */
  const char* orig_filename = strrchr(db->path, '/');
  if (orig_filename) {
    orig_filename++; /* Skip the slash */
  } else {
    orig_filename = db->path;
  }
  
  /* Create backup path */
  char* backup_path = (char*)BUFFER_ALLOC(strlen(backup_dir) + strlen(orig_filename) + strlen(timestamp) + 3);
  if (!backup_path) {
    BUFFER_FREE(timestamp);
    return NULL;
  }
  
  sprintf(backup_path, "%s/%s_%s", backup_dir, orig_filename, timestamp);
  BUFFER_FREE(timestamp);
  
  /* Export database to backup path */
  int result = export_database(db, backup_path);
  if (result != EXPORT_SUCCESS) {
    BUFFER_FREE(backup_path);
    return NULL;
  }
  
  return backup_path;
}

/* Error message for export error code */
const char* export_error_message(int error_code) {
  switch (error_code) {
    case EXPORT_SUCCESS:
      return "Export successful";
    case EXPORT_ERROR_INVALID_ARGS:
      return "Invalid arguments";
    case EXPORT_ERROR_MEMORY:
      return "Memory allocation error";
    case EXPORT_ERROR_DIRECTORY:
      return "Failed to create directory";
    case EXPORT_ERROR_DATABASE_SAVE:
      return "Failed to save database";
    case EXPORT_ERROR_SOURCE_FILE:
      return "Failed to open source file";
    case EXPORT_ERROR_DESTINATION_FILE:
      return "Failed to open destination file";
    case EXPORT_ERROR_WRITE:
      return "Failed to write to destination file";
    default:
      return "Unknown error";
  }
}

/* Error message for import error code */
const char* import_error_message(int error_code) {
  switch (error_code) {
    case IMPORT_SUCCESS:
      return "Import successful";
    case IMPORT_ERROR_INVALID_ARGS:
      return "Invalid arguments";
    case IMPORT_ERROR_MEMORY:
      return "Memory allocation error";
    case IMPORT_ERROR_SOURCE_FILE:
      return "Failed to open source file";
    case IMPORT_ERROR_INVALID_FORMAT:
      return "Invalid JSON format";
    case IMPORT_ERROR_NOT_EMPTY:
      return "Database is not empty";
    case IMPORT_ERROR_DATABASE_SAVE:
      return "Failed to save database";
    default:
      return "Unknown error";
  }
}