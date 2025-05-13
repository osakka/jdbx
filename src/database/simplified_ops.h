#ifndef SIMPLIFIED_OPS_H
#define SIMPLIFIED_OPS_H

#include "jsondb/database/database.h"

/* Simplified, deadlock-free database operations */
json_value_t* simplified_db_insert_document(database_t* db, const char* collection_name, json_value_t* document);
json_value_t* simplified_db_get_document(database_t* db, const char* collection_name, const char* id);
json_value_t* simplified_db_update_document(database_t* db, const char* collection_name, const char* id, json_value_t* document);
int simplified_db_delete_document(database_t* db, const char* collection_name, const char* id);
json_value_t* simplified_db_query_documents(database_t* db, const char* collection_name, json_value_t* query_json);

#endif /* SIMPLIFIED_OPS_H */