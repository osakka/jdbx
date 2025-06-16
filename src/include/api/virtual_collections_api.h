#ifndef VIRTUAL_COLLECTIONS_API_H
#define VIRTUAL_COLLECTIONS_API_H

#include "api/api.h"

/**
 * @file virtual_collections_api.h
 * @brief Virtual Collections API - Manages logical collections based on document types
 * 
 * Virtual collections are logical groupings of documents based on their 'type' and 'collection' fields.
 * All data is physically stored in a single unified collection (default/documents).
 */

/* Virtual Collections API handlers */
http_response_t* api_handle_virtual_collections_list(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_virtual_collection_create(api_context_t* ctx, http_request_t* request);
http_response_t* api_handle_virtual_collection_drop(api_context_t* ctx, http_request_t* request);

#endif /* VIRTUAL_COLLECTIONS_API_H */