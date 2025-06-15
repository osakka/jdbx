#include "database/document_storage.h"
#include "database/database.h"
#include "rbac/rbac.h"
#include "utils/logger.h"
#include "utils/json_helpers.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Document type string mappings */
static const struct {
    document_type_t type;
    const char* name;
} type_mappings[] = {
    { DOC_TYPE_USER, "user" },
    { DOC_TYPE_ROLE, "role" },
    { DOC_TYPE_MEMBERSHIP, "membership" },
    { DOC_TYPE_PERMISSION, "permission" },
    { DOC_TYPE_SESSION, "session" },
    { DOC_TYPE_LIBRARY, "library" },
    { DOC_TYPE_COLLECTION, "collection" },
    { DOC_TYPE_FUNCTION, "function" },
    { DOC_TYPE_VALIDATOR, "validator" },
    { DOC_TYPE_TRANSFORMER, "transformer" },
    { DOC_TYPE_METRIC, "metric" },
    { DOC_TYPE_CONFIG, "config" },
    { DOC_TYPE_INDEX, "index" },
    { DOC_TYPE_SCHEMA, "schema" },
    { DOC_TYPE_VERSION, "version" },
    { DOC_TYPE_AUDIT, "audit" },
    { DOC_TYPE_CACHE_ENTRY, "cache_entry" }
};

const char* document_type_to_string(document_type_t type) {
    for (size_t i = 0; i < sizeof(type_mappings) / sizeof(type_mappings[0]); i++) {
        if (type_mappings[i].type == type) {
            return type_mappings[i].name;
        }
    }
    return "unknown";
}

document_type_t document_type_from_string(const char* type_str) {
    if (!type_str) return DOC_TYPE_UNKNOWN;
    
    for (size_t i = 0; i < sizeof(type_mappings) / sizeof(type_mappings[0]); i++) {
        if (strcmp(type_mappings[i].name, type_str) == 0) {
            return type_mappings[i].type;
        }
    }
    return DOC_TYPE_UNKNOWN;
}

/* Create system actor user */
static int create_system_actor(database_t* db, const char* username, const char* description,
                              const char* permissions_json) {
    LOG_INFO("Creating system actor: %s", username);
    
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("user"));
    json_object_set(query, "name", json_create_string(username));
    
    json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    if (results) {
        json_value_t* docs = json_object_get(results, "documents");
        if (docs && docs->type == JSON_ARRAY && json_array_size(docs) > 0) {
            LOG_DEBUG("System actor %s already exists", username);
            json_free(results);
            return 1;
        }
        json_free(results);
    }
    
    /* Create system actor document */
    json_value_t* actor = json_create_object();
    
    /* Standard document fields */
    json_object_set(actor, "type", json_create_string("user"));
    json_object_set(actor, "name", json_create_string(username));
    json_object_set(actor, "library", json_create_string("system"));
    json_object_set(actor, "collection", json_create_string("users"));
    
    /* User-specific fields */
    json_object_set(actor, "username", json_create_string(username));
    json_object_set(actor, "email", json_create_string(username));
    json_object_set(actor, "full_name", json_create_string(description));
    json_object_set(actor, "is_system", json_create_boolean(1));
    json_object_set(actor, "active", json_create_boolean(1));
    
    /* No password for system actors - they can't login */
    json_object_set(actor, "password_hash", json_create_null());
    
    /* Timestamps - use Unix timestamp integers for consistency */
    time_t now = time(NULL);
    json_object_set(actor, "created_at", json_create_integer(now));
    json_object_set(actor, "updated_at", json_create_integer(now));
    
    /* Owner is self */
    json_object_set(actor, "owner", json_create_string(username));
    
    /* Permissions */
    if (permissions_json) {
        json_value_t* perms = json_parse(permissions_json);
        if (perms) {
            json_object_set(actor, "permissions", perms);
        }
    }
    
    /* Insert */
    json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, actor);
    json_free(actor);
    
    if (result) {
        LOG_INFO("Created system actor: %s", username);
        json_free(result);
        return 1;
    }
    
    LOG_ERROR("Failed to create system actor: %s", username);
    return 0;
}

/* Create collection metadata helper */
__attribute__((unused)) static int create_collection_metadata(database_t* db, const char* library, const char* collection_name,
                                     const char* owner, int is_system) {
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("collection"));
    json_object_set(query, "name", json_create_string(collection_name));
    json_object_set(query, "library", json_create_string(library));
    
    json_value_t* existing = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "documents") &&
        json_object_get(existing, "documents")->value.array.size > 0) {
        json_free(existing);
        return 1; /* Already exists */
    }
    if (existing) json_free(existing);
    
    /* Create collection metadata */
    json_value_t* coll = json_create_object();
    add_document_system_fields(coll, "collection", library, "collections", owner);
    json_object_set(coll, "name", json_create_string(collection_name));
    json_object_set(coll, "library", json_create_string(library));
    json_object_set(coll, "is_system", json_create_boolean(is_system));
    
    /* Add versioning policy */
    json_value_t* versioning = json_create_object();
    json_object_set(versioning, "enabled", json_create_boolean(1));
    json_object_set(versioning, "max_versions", json_create_number(10));
    json_object_set(coll, "versioning", versioning);
    
    /* Add collection-specific settings */
    json_value_t* settings = json_create_object();
    if (strcmp(collection_name, "users") == 0) {
        json_object_set(settings, "unique_fields", json_create_array());
        json_array_append(json_object_get(settings, "unique_fields"), json_create_string("username"));
    }
    json_object_set(coll, "settings", settings);
    
    json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, coll);
    json_free(coll);
    
    if (result) {
        LOG_DEBUG("Created collection metadata: %s.%s", library, collection_name);
        json_free(result);
        
        /* Also create the actual collection in the library */
        char collection_path[256];
        snprintf(collection_path, sizeof(collection_path), "%s/%s", library, collection_name);
        
        if (!db_collection_exists(db, collection_path)) {
            if (db_create_collection(db, collection_path) != 0) {
                LOG_ERROR("Failed to create physical collection: %s", collection_path);
                return 0;
            }
            LOG_DEBUG("Created physical collection: %s", collection_path);
        }
        
        return 1;
    }
    
    return 0;
}

/* Create library metadata document */
__attribute__((unused)) static int create_library_metadata(database_t* db, const char* name, const char* display_name,
                                  const char* description, const char* owner, int is_system) {
    /* Check if already exists */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("library"));
    json_object_set(query, "name", json_create_string(name));
    
    json_value_t* existing = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    if (existing && json_object_get(existing, "documents") &&
        json_object_get(existing, "documents")->value.array.size > 0) {
        json_free(existing);
        return 1; /* Already exists */
    }
    if (existing) json_free(existing);
    
    /* Create library metadata */
    json_value_t* lib = json_create_object();
    json_object_set(lib, "type", json_create_string("library"));
    json_object_set(lib, "name", json_create_string(name));
    json_object_set(lib, "display_name", json_create_string(display_name));
    json_object_set(lib, "description", json_create_string(description));
    json_object_set(lib, "owner", json_create_string(owner));
    json_object_set(lib, "is_system", json_create_boolean(is_system));
    
    /* Library settings */
    json_value_t* settings = json_create_object();
    json_object_set(settings, "default_versioning", json_create_boolean(1));
    json_object_set(settings, "max_collections", json_create_number(is_system ? -1 : 100));
    json_object_set(settings, "max_storage", json_create_string(is_system ? "unlimited" : "10GB"));
    json_object_set(lib, "settings", settings);
    
    /* Add timestamps */
    add_document_system_fields(lib, "library", "system", "libraries", owner);
    
    json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, lib);
    json_free(lib);
    
    if (result) {
        LOG_INFO("Created library metadata: %s", name);
        json_free(result);
        
        /* The database layer will handle creating directories when collections are created */
        LOG_DEBUG("Library metadata created for: %s", name);
        
        return 1;
    }
    
    return 0;
}

/* Create all system actors */
int create_system_actors(database_t* db) {
    LOG_INFO("Creating system actors");
    
    /* System admin - owns system library and collections */
    create_system_actor(db, SYSTEM_USER_ADMIN, "System Administrator",
        "{\"*\": [\"read\", \"write\", \"delete\", \"execute\", \"admin\"]}");
    
    /* Metrics collector - can write to system/metrics collection */
    create_system_actor(db, SYSTEM_USER_METRICS, "Metrics Collector",
        "{\"system/metrics\": [\"write\"], \"documents\": [\"read\"]}");
    
    /* Indexer - can manage indexes and read all collections */
    create_system_actor(db, SYSTEM_USER_INDEXER, "Index Manager",
        "{\"*/*\": [\"read\"], \"documents\": [\"read\", \"write\"]}");
    
    /* Persistence - can read all collections for backup */
    create_system_actor(db, SYSTEM_USER_PERSISTENCE, "Persistence Manager",
        "{\"*/*\": [\"read\"], \"documents\": [\"read\"], \"binary\": [\"write\"]}");
    
    /* Cache manager - manages cache metadata */
    create_system_actor(db, SYSTEM_USER_CACHE, "Cache Manager",
        "{\"documents\": [\"read\", \"write\"]}");
    
    /* Audit logger - writes to system/audit */
    create_system_actor(db, SYSTEM_USER_AUDIT, "Audit Logger",
        "{\"system/audit\": [\"write\"], \"documents\": [\"read\"]}");
    
    return 1;
}

/* Create predefined library templates */
int create_library_templates(database_t* db) {
    LOG_INFO("Creating predefined library templates");
    
    /* E-Commerce Template */
    json_value_t* ecommerce_template = json_create_object();
    json_object_set(ecommerce_template, "type", json_create_string("library_template"));
    json_object_set(ecommerce_template, "name", json_create_string("e-commerce"));
    json_object_set(ecommerce_template, "display_name", json_create_string("E-Commerce Template"));
    json_object_set(ecommerce_template, "description", json_create_string("Complete e-commerce platform with products, orders, customers, and inventory management"));
    
    /* E-commerce collections */
    json_value_t* ecommerce_collections = json_create_array();
    
    /* Products collection */
    json_value_t* products_collection = json_create_object();
    json_object_set(products_collection, "name", json_create_string("products"));
    json_object_set(products_collection, "display_name", json_create_string("Products"));
    json_object_set(products_collection, "description", json_create_string("Product catalog"));
    json_value_t* products_schema = json_create_object();
    json_object_set(products_schema, "type", json_create_string("object"));
    json_value_t* products_props = json_create_object();
    json_value_t* name_prop = json_create_object();
    json_object_set(name_prop, "type", json_create_string("string"));
    json_object_set(products_props, "name", name_prop);
    json_value_t* price_prop = json_create_object();
    json_object_set(price_prop, "type", json_create_string("number"));
    json_object_set(products_props, "price", price_prop);
    json_value_t* category_prop = json_create_object();
    json_object_set(category_prop, "type", json_create_string("string"));
    json_object_set(products_props, "category", category_prop);
    json_value_t* inventory_prop = json_create_object();
    json_object_set(inventory_prop, "type", json_create_string("integer"));
    json_object_set(products_props, "inventory", inventory_prop);
    json_object_set(products_schema, "properties", products_props);
    json_object_set(products_collection, "schema", products_schema);
    json_array_append(ecommerce_collections, products_collection);
    
    /* Orders collection */
    json_value_t* orders_collection = json_create_object();
    json_object_set(orders_collection, "name", json_create_string("orders"));
    json_object_set(orders_collection, "display_name", json_create_string("Orders"));
    json_object_set(orders_collection, "description", json_create_string("Customer orders"));
    json_value_t* orders_schema = json_create_object();
    json_object_set(orders_schema, "type", json_create_string("object"));
    json_value_t* orders_props = json_create_object();
    json_value_t* customer_id_prop = json_create_object();
    json_object_set(customer_id_prop, "type", json_create_string("string"));
    json_object_set(orders_props, "customer_id", customer_id_prop);
    json_value_t* total_prop = json_create_object();
    json_object_set(total_prop, "type", json_create_string("number"));
    json_object_set(orders_props, "total", total_prop);
    json_value_t* status_prop = json_create_object();
    json_object_set(status_prop, "type", json_create_string("string"));
    json_object_set(orders_props, "status", status_prop);
    json_object_set(orders_schema, "properties", orders_props);
    json_object_set(orders_collection, "schema", orders_schema);
    json_array_append(ecommerce_collections, orders_collection);
    
    /* Customers collection */
    json_value_t* customers_collection = json_create_object();
    json_object_set(customers_collection, "name", json_create_string("customers"));
    json_object_set(customers_collection, "display_name", json_create_string("Customers"));
    json_object_set(customers_collection, "description", json_create_string("Customer information"));
    json_value_t* customers_schema = json_create_object();
    json_object_set(customers_schema, "type", json_create_string("object"));
    json_value_t* customers_props = json_create_object();
    json_value_t* email_prop = json_create_object();
    json_object_set(email_prop, "type", json_create_string("string"));
    json_object_set(customers_props, "email", email_prop);
    json_value_t* full_name_prop = json_create_object();
    json_object_set(full_name_prop, "type", json_create_string("string"));
    json_object_set(customers_props, "full_name", full_name_prop);
    json_object_set(customers_schema, "properties", customers_props);
    json_object_set(customers_collection, "schema", customers_schema);
    json_array_append(ecommerce_collections, customers_collection);
    
    /* Categories collection */
    json_value_t* categories_collection = json_create_object();
    json_object_set(categories_collection, "name", json_create_string("categories"));
    json_object_set(categories_collection, "display_name", json_create_string("Categories"));
    json_object_set(categories_collection, "description", json_create_string("Product categories"));
    json_array_append(ecommerce_collections, categories_collection);
    
    json_object_set(ecommerce_template, "collections", ecommerce_collections);
    
    /* E-commerce settings */
    json_value_t* ecommerce_settings = json_create_object();
    json_object_set(ecommerce_settings, "default_currency", json_create_string("USD"));
    json_object_set(ecommerce_settings, "inventory_tracking", json_create_boolean(1));
    json_object_set(ecommerce_settings, "order_notifications", json_create_boolean(1));
    json_object_set(ecommerce_template, "settings", ecommerce_settings);
    
    add_document_system_fields(ecommerce_template, "library_template", "system", "templates", SYSTEM_USER_ADMIN);
    
    /* Wiki Template */
    json_value_t* wiki_template = json_create_object();
    json_object_set(wiki_template, "type", json_create_string("library_template"));
    json_object_set(wiki_template, "name", json_create_string("wiki"));
    json_object_set(wiki_template, "display_name", json_create_string("Wiki Template"));
    json_object_set(wiki_template, "description", json_create_string("Knowledge base and documentation platform with pages, categories, and revision history"));
    
    /* Wiki collections */
    json_value_t* wiki_collections = json_create_array();
    
    /* Pages collection */
    json_value_t* pages_collection = json_create_object();
    json_object_set(pages_collection, "name", json_create_string("pages"));
    json_object_set(pages_collection, "display_name", json_create_string("Pages"));
    json_object_set(pages_collection, "description", json_create_string("Wiki pages"));
    json_value_t* pages_schema = json_create_object();
    json_object_set(pages_schema, "type", json_create_string("object"));
    json_value_t* pages_props = json_create_object();
    json_value_t* title_prop = json_create_object();
    json_object_set(title_prop, "type", json_create_string("string"));
    json_object_set(pages_props, "title", title_prop);
    json_value_t* content_prop = json_create_object();
    json_object_set(content_prop, "type", json_create_string("string"));
    json_object_set(pages_props, "content", content_prop);
    json_value_t* tags_prop = json_create_object();
    json_object_set(tags_prop, "type", json_create_string("array"));
    json_object_set(pages_props, "tags", tags_prop);
    json_object_set(pages_schema, "properties", pages_props);
    json_object_set(pages_collection, "schema", pages_schema);
    json_array_append(wiki_collections, pages_collection);
    
    /* Revisions collection */
    json_value_t* revisions_collection = json_create_object();
    json_object_set(revisions_collection, "name", json_create_string("revisions"));
    json_object_set(revisions_collection, "display_name", json_create_string("Revisions"));
    json_object_set(revisions_collection, "description", json_create_string("Page revision history"));
    json_array_append(wiki_collections, revisions_collection);
    
    /* Wiki categories collection */
    json_value_t* wiki_categories_collection = json_create_object();
    json_object_set(wiki_categories_collection, "name", json_create_string("categories"));
    json_object_set(wiki_categories_collection, "display_name", json_create_string("Categories"));
    json_object_set(wiki_categories_collection, "description", json_create_string("Content categories"));
    json_array_append(wiki_collections, wiki_categories_collection);
    
    /* Comments collection */
    json_value_t* comments_collection = json_create_object();
    json_object_set(comments_collection, "name", json_create_string("comments"));
    json_object_set(comments_collection, "display_name", json_create_string("Comments"));
    json_object_set(comments_collection, "description", json_create_string("Page comments and discussions"));
    json_array_append(wiki_collections, comments_collection);
    
    json_object_set(wiki_template, "collections", wiki_collections);
    
    /* Wiki settings */
    json_value_t* wiki_settings = json_create_object();
    json_object_set(wiki_settings, "enable_comments", json_create_boolean(1));
    json_object_set(wiki_settings, "enable_revisions", json_create_boolean(1));
    json_object_set(wiki_settings, "markdown_support", json_create_boolean(1));
    json_object_set(wiki_template, "settings", wiki_settings);
    
    add_document_system_fields(wiki_template, "library_template", "system", "templates", SYSTEM_USER_ADMIN);
    
    /* CMS Template */
    json_value_t* cms_template = json_create_object();
    json_object_set(cms_template, "type", json_create_string("library_template"));
    json_object_set(cms_template, "name", json_create_string("cms"));
    json_object_set(cms_template, "display_name", json_create_string("Content Management System"));
    json_object_set(cms_template, "description", json_create_string("Full-featured content management system with posts, media, menus, and publishing workflow"));
    
    /* CMS collections */
    json_value_t* cms_collections = json_create_array();
    
    /* Posts collection */
    json_value_t* posts_collection = json_create_object();
    json_object_set(posts_collection, "name", json_create_string("posts"));
    json_object_set(posts_collection, "display_name", json_create_string("Posts"));
    json_object_set(posts_collection, "description", json_create_string("Blog posts and articles"));
    json_value_t* posts_schema = json_create_object();
    json_object_set(posts_schema, "type", json_create_string("object"));
    json_value_t* posts_props = json_create_object();
    json_value_t* post_title_prop = json_create_object();
    json_object_set(post_title_prop, "type", json_create_string("string"));
    json_object_set(posts_props, "title", post_title_prop);
    json_value_t* post_content_prop = json_create_object();
    json_object_set(post_content_prop, "type", json_create_string("string"));
    json_object_set(posts_props, "content", post_content_prop);
    json_value_t* post_status_prop = json_create_object();
    json_object_set(post_status_prop, "type", json_create_string("string"));
    json_object_set(posts_props, "status", post_status_prop);
    json_object_set(posts_schema, "properties", posts_props);
    json_object_set(posts_collection, "schema", posts_schema);
    json_array_append(cms_collections, posts_collection);
    
    /* Media collection */
    json_value_t* media_collection = json_create_object();
    json_object_set(media_collection, "name", json_create_string("media"));
    json_object_set(media_collection, "display_name", json_create_string("Media"));
    json_object_set(media_collection, "description", json_create_string("Images, videos, and documents"));
    json_array_append(cms_collections, media_collection);
    
    /* Menus collection */
    json_value_t* menus_collection = json_create_object();
    json_object_set(menus_collection, "name", json_create_string("menus"));
    json_object_set(menus_collection, "display_name", json_create_string("Menus"));
    json_object_set(menus_collection, "description", json_create_string("Navigation menus"));
    json_array_append(cms_collections, menus_collection);
    
    /* Pages for CMS */
    json_value_t* cms_pages_collection = json_create_object();
    json_object_set(cms_pages_collection, "name", json_create_string("pages"));
    json_object_set(cms_pages_collection, "display_name", json_create_string("Pages"));
    json_object_set(cms_pages_collection, "description", json_create_string("Static pages"));
    json_array_append(cms_collections, cms_pages_collection);
    
    /* Themes collection */
    json_value_t* themes_collection = json_create_object();
    json_object_set(themes_collection, "name", json_create_string("themes"));
    json_object_set(themes_collection, "display_name", json_create_string("Themes"));
    json_object_set(themes_collection, "description", json_create_string("Site themes and templates"));
    json_array_append(cms_collections, themes_collection);
    
    json_object_set(cms_template, "collections", cms_collections);
    
    /* CMS settings */
    json_value_t* cms_settings = json_create_object();
    json_object_set(cms_settings, "enable_publishing", json_create_boolean(1));
    json_object_set(cms_settings, "enable_drafts", json_create_boolean(1));
    json_object_set(cms_settings, "enable_comments", json_create_boolean(1));
    json_object_set(cms_settings, "default_theme", json_create_string("default"));
    json_object_set(cms_template, "settings", cms_settings);
    
    add_document_system_fields(cms_template, "library_template", "system", "templates", SYSTEM_USER_ADMIN);
    
    /* Insert templates into database */
    json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, ecommerce_template);
    if (result) {
        LOG_INFO("Created e-commerce library template");
        json_free(result);
    }
    json_free(ecommerce_template);
    
    result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, wiki_template);
    if (result) {
        LOG_INFO("Created wiki library template");
        json_free(result);
    }
    json_free(wiki_template);
    
    result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, cms_template);
    if (result) {
        LOG_INFO("Created CMS library template");
        json_free(result);
    }
    json_free(cms_template);
    
    return 1;
}

/* Initialize unified documents system */
int unified_documents_init(database_t* db) {
    if (!db) return 0;
    
    LOG_INFO("Initializing unified documents system");
    
    /* CRITICAL: Create documents collection FIRST before any other operations */
    if (!db_collection_exists(db, DOCUMENTS_COLLECTION)) {
        /* Use simple collection name to ensure it's at root level */
        if (db_create_collection(db, DOCUMENTS_COLLECTION) != 0) {
            LOG_ERROR("Failed to create documents collection");
            return 0;
        }
        LOG_INFO("Created documents collection at root level");
    } else {
        LOG_INFO("Documents collection already exists");
    }
    
    /* Create indexes AFTER collection exists */
    LOG_INFO("Creating unified document indexes");
    
    /* Type index for fast filtering */
    db_create_index(db, STORAGE_COLLECTION, "idx_type", "type", INDEX_TYPE_NON_UNIQUE);
    
    /* Compound indexes for common queries */
    db_create_index(db, STORAGE_COLLECTION, "idx_type_name", "type,name", INDEX_TYPE_UNIQUE);
    db_create_index(db, STORAGE_COLLECTION, "idx_type_lib_coll", "type,library,collection", INDEX_TYPE_NON_UNIQUE);
    
    /* Library and collection indexes */
    db_create_index(db, STORAGE_COLLECTION, "idx_library", "library", INDEX_TYPE_NON_UNIQUE);
    db_create_index(db, STORAGE_COLLECTION, "idx_collection", "collection", INDEX_TYPE_NON_UNIQUE);
    
    /* Owner index for ownership queries */
    db_create_index(db, STORAGE_COLLECTION, "idx_owner", "owner", INDEX_TYPE_NON_UNIQUE);
    
    /* Username index for users */
    db_create_index(db, STORAGE_COLLECTION, "idx_username", "username", INDEX_TYPE_UNIQUE);
    
    /* Create system actors */
    create_system_actors(db);
    
    /* PURE DOCUMENTS: No library/collection creation - everything in documents collection */
    LOG_INFO("Skipping library/collection creation - using pure documents architecture");
    
    /* Create admin role */
    json_value_t* query = json_create_object();
    json_object_set(query, "type", json_create_string("role"));
    json_object_set(query, "name", json_create_string("admin"));
    json_value_t* existing = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    char* admin_role_id = NULL;
    if (!existing || !json_object_get(existing, "documents") ||
        json_object_get(existing, "documents")->value.array.size == 0) {
        /* Create admin role */
        json_value_t* admin_role = json_create_object();
        add_document_system_fields(admin_role, "role", "system", "roles", SYSTEM_USER_ADMIN);
        json_object_set(admin_role, "name", json_create_string("admin"));
        json_object_set(admin_role, "display_name", json_create_string("Administrator"));
        json_object_set(admin_role, "description", json_create_string("Full system access"));
        
        /* Admin has all permissions on all resources */
        json_value_t* permissions = json_create_object();
        json_value_t* all_perms = json_create_array();
        json_array_append(all_perms, json_create_string("read"));
        json_array_append(all_perms, json_create_string("write"));
        json_array_append(all_perms, json_create_string("delete"));
        json_array_append(all_perms, json_create_string("execute"));
        json_array_append(all_perms, json_create_string("admin"));
        json_object_set(permissions, "*/*", all_perms);
        json_object_set(admin_role, "permissions", permissions);
        
        json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, admin_role);
        if (result) {
            json_value_t* id_val = json_object_get(result, "uuid");
            if (id_val && id_val->type == JSON_STRING) {
                admin_role_id = strdup(id_val->value.string);
            }
            json_free(result);
        }
        json_free(admin_role);
        LOG_INFO("Created admin role");
    }
    if (existing) json_free(existing);
    
    /* PURE DOCUMENTS: Create admin user in documents collection */
    query = json_create_object();
    json_object_set(query, "type", json_create_string("user"));
    json_object_set(query, "username", json_create_string("admin"));
    json_object_set(query, "library", json_create_string("system"));
    existing = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    if (!existing || !json_object_get(existing, "documents") ||
        json_object_get(existing, "documents")->value.array.size == 0) {
        /* Create admin user with proper document structure */
        json_value_t* admin_user = json_create_object();
        
        /* PURE DOCUMENTS: Add type and library fields */
        json_object_set(admin_user, "type", json_create_string("user"));
        json_object_set(admin_user, "library", json_create_string("system"));
        json_object_set(admin_user, "username", json_create_string("admin"));
        json_object_set(admin_user, "email", json_create_string("admin@localhost"));
        json_object_set(admin_user, "full_name", json_create_string("System Administrator"));
        
        /* Hash password "admin" for compatibility with authentication_handler.c */
        extern char* hash_password(const char* password);
        char* password_hash = hash_password("admin");
        json_object_set(admin_user, "password_hash", json_create_string(password_hash));
        free(password_hash);
        
        json_object_set(admin_user, "status", json_create_string("active"));
        json_object_set(admin_user, "is_admin", json_create_boolean(1));
        json_object_set(admin_user, "active", json_create_boolean(1));
        json_object_set(admin_user, "roles", json_create_array());
        
        /* Add admin role */
        if (admin_role_id) {
            json_array_append(json_object_get(admin_user, "roles"), json_create_string(admin_role_id));
        }
        
        /* Add system fields for unified documents */
        add_document_system_fields(admin_user, "user", "system", "users", SYSTEM_USER_ADMIN);
        
        json_value_t* result = db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, admin_user);
        if (result) {
            LOG_INFO("Created admin user with password 'admin' in documents collection");
            json_free(result);
        }
        json_free(admin_user);
    }
    if (existing) json_free(existing);
    if (admin_role_id) free(admin_role_id);
    
    /* Create predefined library templates */
    create_library_templates(db);
    
    LOG_INFO("Unified documents system initialized");
    return 1;
}

/* Query documents by type */
json_value_t* query_documents_by_type(database_t* db, const char* type,
                                     json_value_t* additional_query) {
    if (!db || !type) return NULL;
    
    json_value_t* query = additional_query ? json_clone(additional_query) : json_create_object();
    json_object_set(query, "type", json_create_string(type));
    
    json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    return results;
}

/* Query documents by type and library/collection */
json_value_t* query_documents_by_location(database_t* db, const char* type,
                                         const char* library, const char* collection,
                                         json_value_t* additional_query) {
    if (!db || !type) return NULL;
    
    json_value_t* query = additional_query ? json_clone(additional_query) : json_create_object();
    json_object_set(query, "type", json_create_string(type));
    
    if (library) {
        json_object_set(query, "library", json_create_string(library));
    }
    
    if (collection) {
        json_object_set(query, "collection", json_create_string(collection));
    }
    
    json_value_t* results = db_query_documents(db, STORAGE_LIBRARY, STORAGE_COLLECTION, query);
    json_free(query);
    
    return results;
}

/* Add system fields to document */
void add_document_system_fields(json_value_t* doc, const char* type,
                               const char* library, const char* collection,
                               const char* owner_id) {
    if (!doc) return;
    
    /* Type is mandatory */
    if (type) {
        json_object_set(doc, "type", json_create_string(type));
    }
    
    /* Library and collection for organization */
    if (library) {
        json_object_set(doc, "library", json_create_string(library));
    }
    
    if (collection) {
        json_object_set(doc, "collection", json_create_string(collection));
    }
    
    /* Owner */
    if (owner_id) {
        json_object_set(doc, "owner", json_create_string(owner_id));
    }
    
    /* Timestamps */
    time_t now = time(NULL);
    
    if (!json_object_get(doc, "created_at")) {
        json_object_set(doc, "created_at", json_create_integer(now));
    }
    json_object_set(doc, "updated_at", json_create_integer(now));
    
    /* Version if not set */
    if (!json_object_get(doc, "version")) {
        json_object_set(doc, "version", json_create_number(1));
    }
}

/* Create document with proper type and ownership */
json_value_t* create_typed_document(database_t* db, const char* type,
                                   const char* name, const char* owner_id,
                                   json_value_t* content) {
    if (!db || !type || !name || !owner_id) return NULL;
    
    json_value_t* doc = content ? json_clone(content) : json_create_object();
    
    /* Set name */
    json_object_set(doc, "name", json_create_string(name));
    
    /* Add system fields */
    add_document_system_fields(doc, type, NULL, NULL, owner_id);
    
    /* Insert document */
    return db_insert_document(db, STORAGE_LIBRARY, STORAGE_COLLECTION, doc);
}

/* Validate document has required fields */
int validate_document_structure(json_value_t* doc) {
    if (!doc || doc->type != JSON_OBJECT) return 0;
    
    /* Required fields */
    json_value_t* type = json_object_get(doc, "type");
    if (!type || type->type != JSON_STRING) {
        LOG_ERROR("Document missing required 'type' field");
        return 0;
    }
    
    json_value_t* owner = json_object_get(doc, "owner");
    if (!owner || owner->type != JSON_STRING) {
        LOG_ERROR("Document missing required 'owner' field");
        return 0;
    }
    
    return 1;
}