/**
 * Configuration-specific string interning for JSONdb
 * 
 * Provides specialized string pools for different types of strings
 * to achieve maximum performance benefits from string interning.
 */

#ifndef CONFIG_STRING_POOL_H
#define CONFIG_STRING_POOL_H

#include <stddef.h>

/* Initialize specialized string pools */
void config_string_pools_init(void);

/* Cleanup specialized string pools */
void config_string_pools_cleanup(void);

/* Intern different types of strings */
const char* config_string_intern(const char* str);
const char* json_key_intern(const char* str);
const char* doc_id_intern(const char* str);
const char* error_message_intern(const char* str);

/* Get statistics for all specialized pools */
void config_string_pools_get_stats(
    size_t* config_count, size_t* config_memory,
    size_t* json_keys_count, size_t* json_keys_memory,
    size_t* doc_ids_count, size_t* doc_ids_memory,
    size_t* errors_count, size_t* errors_memory
);

#endif /* CONFIG_STRING_POOL_H */