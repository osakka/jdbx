#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "core/server.h"
#include "utils/logger.h"

/* Global configuration structure */
extern server_config_t* g_server_config;

/* Load configuration from file (auto-detects format) */
int config_load(const char* filepath, server_config_t* config);

/* Load configuration from JSON file */
int config_load_json(const char* filepath, server_config_t* config);

/* Load configuration from key=value file */
int config_load_keyvalue(const char* filepath, server_config_t* config);

/* Free configuration resources */
void config_free(server_config_t* config);

/* Initialize configuration with default values */
void config_init_defaults(server_config_t* config);

#endif /* CONFIG_LOADER_H */