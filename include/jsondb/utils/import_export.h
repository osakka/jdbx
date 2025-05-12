#ifndef IMPORT_EXPORT_H
#define IMPORT_EXPORT_H

#include "database/database.h"

/* Export status codes */
#define EXPORT_SUCCESS                  0
#define EXPORT_ERROR_INVALID_ARGS       1
#define EXPORT_ERROR_MEMORY             2
#define EXPORT_ERROR_DIRECTORY          3
#define EXPORT_ERROR_DATABASE_SAVE      4
#define EXPORT_ERROR_SOURCE_FILE        5
#define EXPORT_ERROR_DESTINATION_FILE   6
#define EXPORT_ERROR_WRITE              7

/* Import status codes */
#define IMPORT_SUCCESS                  0
#define IMPORT_ERROR_INVALID_ARGS       1
#define IMPORT_ERROR_MEMORY             2
#define IMPORT_ERROR_SOURCE_FILE        3
#define IMPORT_ERROR_INVALID_FORMAT     4
#define IMPORT_ERROR_NOT_EMPTY          5
#define IMPORT_ERROR_DATABASE_SAVE      6

/* Export the database to a file */
int export_database(database_t* db, const char* output_path);

/* Export the database with collections filter */
int export_collections(database_t* db, const char* output_path, const char** collections, int num_collections);

/* Import a database from a file */
int import_database(database_t* db, const char* input_path, int overwrite);

/* Import specific collections */
int import_collections(database_t* db, const char* input_path, const char** collections, int num_collections, int overwrite);

/* Create a backup of the database */
char* backup_database(database_t* db, const char* backup_dir);

/* Error message for export error code */
const char* export_error_message(int error_code);

/* Error message for import error code */
const char* import_error_message(int error_code);

#endif /* IMPORT_EXPORT_H */