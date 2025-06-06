#ifndef DB_OPERATIONS_H
#define DB_OPERATIONS_H

#include "database/database.h"

/* Simplified, deadlock-free database operations */
json_value_t* db_insert_document(database_t* db, const char* collection_name, json_value_t* document);
json_value_t* db_get_document(database_t* db, const char* collection_name, const char* id);
json_value_t* db_update_document(database_t* db, const char* collection_name, const char* id, json_value_t* document);
int db_delete_document(database_t* db, const char* collection_name, const char* id);
json_value_t* db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json);

#endif /* DB_OPERATIONS_H */
