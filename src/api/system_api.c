#include "api/api.h"
#include "database/database.h"
#include "utils/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <time.h>

/* Get CPU information */
static json_value_t* get_cpu_info() {
    json_value_t* cpu = json_create_object();
    
    /* Open /proc/cpuinfo */
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) {
        json_object_set(cpu, "error", json_create_string("Failed to read CPU info"));
        return cpu;
    }
    
    char line[256];
    char model_name[256] = "Unknown";
    int cpu_cores = 0;
    
    /* Read CPU info */
    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "model name", 10) == 0) {
            char* value = strchr(line, ':');
            if (value) {
                value++; /* Skip colon */
                while (*value == ' ' || *value == '\t') value++; /* Skip whitespace */
                char* end = strchr(value, '\n');
                if (end) *end = '\0';
                strncpy(model_name, value, sizeof(model_name) - 1);
                model_name[sizeof(model_name) - 1] = '\0';
            }
        } else if (strncmp(line, "processor", 9) == 0) {
            cpu_cores++;
        }
    }
    
    fclose(cpuinfo);
    
    /* Set CPU info */
    json_object_set(cpu, "model", json_create_string(model_name));
    json_object_set(cpu, "cores", json_create_integer(cpu_cores));
    
    return cpu;
}

/* Get memory information */
static json_value_t* get_memory_info() {
    json_value_t* memory = json_create_object();
    
    /* Get system info */
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        json_object_set(memory, "error", json_create_string("Failed to get memory info"));
        return memory;
    }
    
    /* Set memory info */
    json_object_set(memory, "total", json_create_integer((int64_t)info.totalram * info.mem_unit));
    json_object_set(memory, "free", json_create_integer((int64_t)info.freeram * info.mem_unit));
    json_object_set(memory, "used", json_create_integer((int64_t)(info.totalram - info.freeram) * info.mem_unit));
    
    return memory;
}

/* Get disk information */
static json_value_t* get_disk_info() {
    json_value_t* disk = json_create_object();
    
    /* Get disk info */
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) {
        json_object_set(disk, "error", json_create_string("Failed to get disk info"));
        return disk;
    }
    
    /* Calculate disk space */
    uint64_t total = stat.f_blocks * stat.f_frsize;
    uint64_t available = stat.f_bfree * stat.f_frsize;
    uint64_t used = total - available;
    
    /* Set disk info */
    json_object_set(disk, "total", json_create_integer(total));
    json_object_set(disk, "free", json_create_integer(available));
    json_object_set(disk, "used", json_create_integer(used));
    
    return disk;
}

/* Get system information */
static json_value_t* get_system_info() {
    json_value_t* system = json_create_object();
    
    /* Get system name */
    struct utsname name;
    if (uname(&name) != 0) {
        json_object_set(system, "error", json_create_string("Failed to get system info"));
        return system;
    }
    
    /* Set system info */
    json_object_set(system, "sysname", json_create_string(name.sysname));
    json_object_set(system, "release", json_create_string(name.release));
    json_object_set(system, "version", json_create_string(name.version));
    json_object_set(system, "machine", json_create_string(name.machine));
    
    return system;
}

/* Get database statistics */
static json_value_t* get_database_stats(database_t* db) {
    json_value_t* stats = json_create_object();
    
    if (!db) {
        json_object_set(stats, "error", json_create_string("Database not available"));
        return stats;
    }
    
    /* Get collections list */
    json_value_t* collections = db_list_collections(db);
    if (!collections) {
        json_object_set(stats, "error", json_create_string("Failed to list collections"));
        return stats;
    }
    
    /* Count collections */
    size_t collection_count = json_array_size(collections);
    json_object_set(stats, "collection_count", json_create_integer(collection_count));
    
    /* Create collections array */
    json_value_t* collection_stats = json_create_array();
    
    /* Process each collection */
    for (size_t i = 0; i < collection_count; i++) {
        json_value_t* coll_name_val = json_array_get(collections, i);
        if (!coll_name_val || coll_name_val->type != JSON_STRING) {
            continue;
        }
        
        const char* coll_name = coll_name_val->value.string;
        
        /* Get collection */
        db_collection_t* collection = db_get_collection(db, coll_name);
        if (!collection) {
            continue;
        }
        
        /* Get document count */
        size_t doc_count = 0;
        if (collection->documents && collection->documents->type == JSON_ARRAY) {
            doc_count = json_array_size(collection->documents);
        }
        
        /* Create collection stats */
        json_value_t* coll_stats = json_create_object();
        json_object_set(coll_stats, "name", json_create_string(coll_name));
        json_object_set(coll_stats, "document_count", json_create_integer(doc_count));
        json_object_set(coll_stats, "has_schema", json_create_boolean(collection->schema != NULL));
        
        /* Add to collection stats array */
        json_array_append(collection_stats, coll_stats);
    }
    
    /* Add collections stats to result */
    json_object_set(stats, "collections", collection_stats);
    
    /* Free collections list */
    json_free(collections);
    
    return stats;
}

/* Handle system info request */
http_response_t* api_handle_system_info(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return create_http_response(HTTP_BAD_REQUEST, 
                                  "{\"error\":\"Invalid request\"}", "application/json");
    }
    
    /* Create result object */
    json_value_t* result = json_create_object();
    
    /* Add server information */
    json_object_set(result, "server_version", json_create_string("1.0.0"));
    json_object_set(result, "server_name", json_create_string("JSON Database Server"));
    
    /* Add system info */
    json_object_set(result, "system", get_system_info());
    
    /* Add CPU info */
    json_object_set(result, "cpu", get_cpu_info());
    
    /* Add memory info */
    json_object_set(result, "memory", get_memory_info());
    
    /* Add disk info */
    json_object_set(result, "disk", get_disk_info());
    
    /* Add database stats */
    json_object_set(result, "database", get_database_stats(ctx->db));
    
    /* Add timestamp */
    json_object_set(result, "timestamp", json_create_integer(time(NULL)));
    
    /* Serialize result */
    char* result_str = json_stringify(result);
    
    /* Free result */
    json_free(result);
    
    /* Create HTTP response */
    http_response_t* response = create_http_response(HTTP_OK, result_str, "application/json");
    
    /* Free result string */
    free(result_str);
    
    return response;
}