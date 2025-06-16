#include "database/database.h"
#include "utils/import_export.h"
#include "utils/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* Print usage information */
static void print_usage(const char* program_name) {
  printf("Usage: %s <command> [options] [arguments]\n", program_name);
  printf("\n");
  printf("Commands:\n");
  printf(" export <db_path> <output_path> [--collections=<name1,name2,...>]\n");
  printf("  Export database to a file\n");
  printf("\n");
  printf(" import <db_path> <input_path> [--overwrite] [--collections=<name1,name2,...>]\n");
  printf("  Import database from a file\n");
  printf("\n");
  printf(" backup <db_path> [--dir=<backup_dir>]\n");
  printf("  Create a backup of the database\n");
  printf("\n");
  printf(" info <db_path>\n");
  printf("  Show information about the database\n");
  printf("\n");
  printf("Options:\n");
  printf(" --help, -h          Show this help message\n");
  printf(" --collections=<names>, -c  Comma-separated list of collections\n");
  printf(" --overwrite, -o       Overwrite existing data\n");
  printf(" --dir=<path>, -d       Directory for backups\n");
}

/* Split comma-separated list into array of strings */
static char** split_comma_list(const char* list, int* count) {
  if (!list || !count) {
    return NULL;
  }
  
  /* Count items */
  int num_items = 1;
  const char* p = list;
  while (*p) {
    if (*p == ',') {
      num_items++;
    }
    p++;
  }
  
  /* Allocate array */
  char** items = (char**)BUFFER_ALLOC(num_items * sizeof(char*));
  if (!items) {
    return NULL;
  }
  
  /* Split string */
  char* list_copy = BUFFER_STRDUP(list);
  if (!list_copy) {
    BUFFER_FREE(items);
    return NULL;
  }
  
  char* token = strtok(list_copy, ",");
  int i = 0;
  while (token && i < num_items) {
    items[i++] = BUFFER_STRDUP(token);
    token = strtok(NULL, ",");
  }
  
  BUFFER_FREE(list_copy);
  *count = i;
  
  return items;
}

/* Free array of strings */
static void free_string_array(char** array, int count) {
  if (!array) {
    return;
  }
  
  for (int i = 0; i < count; i++) {
    BUFFER_FREE(array[i]);
  }
  
  BUFFER_FREE(array);
}

/* Export command */
static int cmd_export(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "Error: Missing arguments\n");
    return 1;
  }
  
  const char* db_path = argv[2];
  const char* output_path = argv[3];
  
  /* Parse options */
  char* collections_list = NULL;
  
  static struct option long_options[] = {
    {"collections", required_argument, 0, 'c'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  
  int opt;
  int option_index = 0;
  
  optind = 2; /* Start parsing from the second argument */
  
  while ((opt = getopt_long(argc, argv, "c:h", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'c':
        collections_list = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      default:
        fprintf(stderr, "Error: Unknown option\n");
        return 1;
    }
  }
  
  /* Initialize database */
  database_t* db = db_init(db_path);
  if (!db) {
    fprintf(stderr, "Error: Failed to initialize database\n");
    return 1;
  }
  
  int result;
  
  if (collections_list) {
    /* Export specific collections */
    int num_collections;
    char** collections = split_comma_list(collections_list, &num_collections);
    
    if (!collections) {
      fprintf(stderr, "Error: Failed to parse collections list\n");
      db_close(db);
      return 1;
    }
    
    result = export_collections(db, output_path, (const char**)collections, num_collections);
    
    free_string_array(collections, num_collections);
  } else {
    /* Export entire database */
    result = export_database(db, output_path);
  }
  
  /* Close database */
  db_close(db);
  
  if (result != EXPORT_SUCCESS) {
    fprintf(stderr, "Error: %s\n", export_error_message(result));
    return 1;
  }
  
  printf("Database exported successfully to %s\n", output_path);
  return 0;
}

/* Import command */
static int cmd_import(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "Error: Missing arguments\n");
    return 1;
  }
  
  const char* db_path = argv[2];
  const char* input_path = argv[3];
  
  /* Parse options */
  char* collections_list = NULL;
  int overwrite = 0;
  
  static struct option long_options[] = {
    {"collections", required_argument, 0, 'c'},
    {"overwrite", no_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  
  int opt;
  int option_index = 0;
  
  optind = 2; /* Start parsing from the second argument */
  
  while ((opt = getopt_long(argc, argv, "c:oh", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'c':
        collections_list = optarg;
        break;
      case 'o':
        overwrite = 1;
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      default:
        fprintf(stderr, "Error: Unknown option\n");
        return 1;
    }
  }
  
  /* Initialize database */
  database_t* db = db_init(db_path);
  if (!db) {
    fprintf(stderr, "Error: Failed to initialize database\n");
    return 1;
  }
  
  int result;
  
  if (collections_list) {
    /* Import specific collections */
    int num_collections;
    char** collections = split_comma_list(collections_list, &num_collections);
    
    if (!collections) {
      fprintf(stderr, "Error: Failed to parse collections list\n");
      db_close(db);
      return 1;
    }
    
    result = import_collections(db, input_path, (const char**)collections, num_collections, overwrite);
    
    free_string_array(collections, num_collections);
  } else {
    /* Import entire database */
    result = import_database(db, input_path, overwrite);
  }
  
  /* Close database */
  db_close(db);
  
  if (result != IMPORT_SUCCESS) {
    fprintf(stderr, "Error: %s\n", import_error_message(result));
    return 1;
  }
  
  printf("Database imported successfully from %s\n", input_path);
  return 0;
}

/* Backup command */
static int cmd_backup(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Error: Missing arguments\n");
    return 1;
  }
  
  const char* db_path = argv[2];
  
  /* Parse options */
  char* backup_dir = NULL;
  
  static struct option long_options[] = {
    {"dir", required_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };
  
  int opt;
  int option_index = 0;
  
  optind = 2; /* Start parsing from the second argument */
  
  while ((opt = getopt_long(argc, argv, "d:h", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'd':
        backup_dir = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      default:
        fprintf(stderr, "Error: Unknown option\n");
        return 1;
    }
  }
  
  /* Initialize database */
  database_t* db = db_init(db_path);
  if (!db) {
    fprintf(stderr, "Error: Failed to initialize database\n");
    return 1;
  }
  
  /* Create backup */
  char* backup_path = backup_database(db, backup_dir);
  
  /* Close database */
  db_close(db);
  
  if (!backup_path) {
    fprintf(stderr, "Error: Failed to create backup\n");
    return 1;
  }
  
  printf("Database backed up successfully to %s\n", backup_path);
  BUFFER_FREE(backup_path);
  return 0;
}

/* Info command */
static int cmd_info(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Error: Missing arguments\n");
    return 1;
  }
  
  const char* db_path = argv[2];
  
  /* Initialize database */
  database_t* db = db_init(db_path);
  if (!db) {
    fprintf(stderr, "Error: Failed to initialize database\n");
    return 1;
  }
  
  /* Get database info */
  pthread_rwlock_rdlock(&db->rwlock);
  
  /* Count collections */
  int num_collections = db->collections->value.object.size;
  
  /* Count documents */
  int total_documents = 0;
  for (int i = 0; i < num_collections; i++) {
    json_value_t* collection = db->collections->value.object.entries[i].value;
    if (collection->type == JSON_ARRAY) {
      total_documents += collection->value.array.size;
    }
  }
  
  /* Print collection info */
  printf("Database: %s\n", db_path);
  printf("Collections: %d\n", num_collections);
  printf("Total Documents: %d\n", total_documents);
  
  if (num_collections > 0) {
    printf("\nCollection Details:\n");
    printf("--------------------\n");
    
    for (int i = 0; i < num_collections; i++) {
      const char* coll_name = db->collections->value.object.entries[i].key;
      json_value_t* collection = db->collections->value.object.entries[i].value;
      
      if (collection->type == JSON_ARRAY) {
        int doc_count = collection->value.array.size;
        printf("%s: %d document%s\n", coll_name, doc_count, doc_count == 1 ? "" : "s");
      }
    }
  }
  
  pthread_rwlock_unlock(&db->rwlock);
  
  /* Close database */
  db_close(db);
  
  return 0;
}

/* Main function */
int main(int argc, char** argv) {
  /* Check arguments */
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  
  /* Get command */
  const char* command = argv[1];
  
  /* Execute command */
  if (strcmp(command, "export") == 0) {
    return cmd_export(argc, argv);
  } else if (strcmp(command, "import") == 0) {
    return cmd_import(argc, argv);
  } else if (strcmp(command, "backup") == 0) {
    return cmd_backup(argc, argv);
  } else if (strcmp(command, "info") == 0) {
    return cmd_info(argc, argv);
  } else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    print_usage(argv[0]);
    return 0;
  } else {
    fprintf(stderr, "Error: Unknown command '%s'\n", command);
    print_usage(argv[0]);
    return 1;
  }
}