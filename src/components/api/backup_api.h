#ifndef JSONDB_BACKUP_API_H
#define JSONDB_BACKUP_API_H

#include "components/api/api.h"

/**
 * @brief Initialize the backup API service
 * 
 * This function initializes the backup service, reads configuration,
 * and starts automatic backup thread if configured.
 */
void backup_api_init(void);

/**
 * @brief Register backup API endpoints
 * 
 * @param ctx API context to register endpoints with
 */
void register_backup_api_endpoints(api_context_t *ctx);

/**
 * @brief Create a database backup
 * 
 * API endpoint: POST /api/backup
 * Request body (optional): 
 *   {
 *     "prefix": "optional_prefix" // Custom prefix for backup filename
 *   }
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return http_response_t* HTTP response
 */
http_response_t* api_handle_backup_create(api_context_t *ctx, http_request_t *request);

/**
 * @brief List all available backups
 * 
 * API endpoint: GET /api/backup/list
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return http_response_t* HTTP response
 */
http_response_t* api_handle_backup_list(api_context_t *ctx, http_request_t *request);

/**
 * @brief Restore database from a backup
 * 
 * API endpoint: POST /api/backup/restore
 * Request body:
 *   {
 *     "filename": "backup_20230101_120000.json"
 *   }
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return http_response_t* HTTP response
 */
http_response_t* api_handle_backup_restore(api_context_t *ctx, http_request_t *request);

/**
 * @brief Delete a backup file
 * 
 * API endpoint: DELETE /api/backup/{filename}
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return http_response_t* HTTP response
 */
http_response_t* api_handle_backup_delete(api_context_t *ctx, http_request_t *request);

/**
 * @brief Configure backup settings
 * 
 * API endpoint: POST /api/backup/configure
 * Request body:
 *   {
 *     "auto_backup": true/false,       // Enable/disable automatic backups
 *     "interval_hours": 24,           // Backup interval in hours
 *     "retention_count": 10           // Number of backups to keep
 *   }
 * 
 * @param ctx API context
 * @param request HTTP request
 * @return http_response_t* HTTP response
 */
http_response_t* api_handle_backup_configure(api_context_t *ctx, http_request_t *request);

#endif /* JSONDB_BACKUP_API_H */