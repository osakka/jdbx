#ifndef LIBRARY_METADATA_H
#define LIBRARY_METADATA_H

#include <stdbool.h>
#include <stddef.h>
#include "rbac/rbac.h"
#include "utils/json.h"

/* Library metadata document ID */
#define LIBRARY_META_ID "_meta"

/* Library types */
typedef enum {
    LIBRARY_TYPE_SYSTEM,    /* System library - core functionality */
    LIBRARY_TYPE_DEFAULT,   /* Default library - user collections */
    LIBRARY_TYPE_USER       /* User-created libraries */
} library_type_t;

/* Library metadata structure */
typedef struct library_metadata {
    char* library_name;
    library_type_t type;
    char* description;
    
    /* Library-level permissions */
    rbac_permissions_t permissions;
    
    /* Collections in this library */
    char** collections;
    size_t collection_count;
    
    /* Library configuration */
    struct {
        bool allow_collection_creation;
        bool allow_collection_deletion;
        char* default_collection_template;
    } config;
    
    /* Cross-library access rules */
    struct {
        char** allowed_libraries;    /* Libraries that can access this one */
        size_t allowed_count;
        char** denied_libraries;     /* Explicitly denied libraries */
        size_t denied_count;
    } access;
} library_metadata_t;

/* Library management functions */
library_metadata_t* library_metadata_load(struct database* db, const char* library_name);
library_metadata_t* library_metadata_create_system(void);
library_metadata_t* library_metadata_create_default(void);
int library_metadata_save(struct database* db, const char* library_name, library_metadata_t* metadata);
void library_metadata_free(library_metadata_t* metadata);

/* Library collection management */
int library_add_collection(library_metadata_t* metadata, const char* collection_name);
int library_remove_collection(library_metadata_t* metadata, const char* collection_name);
bool library_has_collection(library_metadata_t* metadata, const char* collection_name);

/* Initialize system libraries */
int library_system_init(struct database* db);

#endif /* LIBRARY_METADATA_H */