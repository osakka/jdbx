#ifndef API_DOCUMENTS_H
#define API_DOCUMENTS_H

#include "api/api.h"
#include <stdbool.h>

/**
 * @file api_documents.h
 * @brief Document Operations API Module
 * 
 * This module handles all document-related operations in JDBX, providing a unified
 * interface for document CRUD operations across unified documents, collections, and
 * library-scoped operations. Implements the core of JDBX's unified documents
 * architecture.
 * 
 * Supported Operations:
 * - Unified document operations (/api/documents)
 * - Collection-scoped document operations (/api/collections/name/documents/id)
 * - Library-scoped document operations (/api/libraries/name/collections/name/documents/id)
 * - Field-level access and manipulation
 * - Query operations with filtering and pagination
 * 
 * Architecture:
 * - Single source of truth through unified documents storage
 * - Type-based document discrimination
 * - Virtual collections via field-based grouping
 * - Atomic operations with ACID compliance
 */

/**
 * ============================================================================
 * UNIFIED DOCUMENTS API
 * ============================================================================
 * 
 * These functions handle the core unified documents interface, providing
 * direct access to the unified documents storage without collection abstraction.
 */

/**
 * Handle unified documents query
 * 
 * Endpoint: GET /api/documents
 * Endpoint: POST /api/documents/query
 * 
 * Supports:
 * - Query parameter filtering
 * - JSON body queries
 * - Type-based filtering
 * - Library scoping
 * - Pagination and sorting
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with query parameters or JSON body
 * @return HTTP response with document array or error
 */
http_response_t* api_handle_unified_documents_query(api_context_t* ctx, http_request_t* request);

/**
 * Handle unified document creation
 * 
 * Endpoint: POST /api/documents
 * 
 * Features:
 * - Automatic field population (type, owner, created_at, etc.)
 * - Field validation and type enforcement
 * - UUID generation
 * - Library scoping based on session
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with JSON document body
 * @return HTTP response with created document or error
 */
http_response_t* api_handle_unified_documents_create(api_context_t* ctx, http_request_t* request);

/**
 * Handle unified document retrieval
 * 
 * Endpoint: GET /api/documents/{uuid}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with document UUID in path
 * @return HTTP response with document or 404 error
 */
http_response_t* api_handle_unified_document_get(api_context_t* ctx, http_request_t* request);

/**
 * Handle unified document update
 * 
 * Endpoint: PUT /api/documents/{uuid}
 * 
 * Features:
 * - Partial updates supported
 * - Field validation
 * - Automatic modified_at timestamp
 * - System field protection (uuid, created_at, etc.)
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with JSON update body
 * @return HTTP response with updated document or error
 */
http_response_t* api_handle_unified_document_update(api_context_t* ctx, http_request_t* request);

/**
 * Handle unified document deletion
 * 
 * Endpoint: DELETE /api/documents/{uuid}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with document UUID in path
 * @return HTTP response with success confirmation or error
 */
http_response_t* api_handle_unified_document_delete(api_context_t* ctx, http_request_t* request);

/**
 * ============================================================================
 * COLLECTION-SCOPED DOCUMENTS API
 * ============================================================================
 * 
 * These functions handle collection-scoped document operations, providing
 * virtual collection abstraction over the unified documents storage.
 */

/**
 * Handle collection documents query
 * 
 * Endpoint: GET /api/collections/{collection}/documents
 * Endpoint: GET /api/libraries/{library}/collections/{collection}/documents
 * 
 * Features:
 * - Collection-type mapping
 * - Library scoping
 * - Virtual collection queries
 * - Pagination and filtering
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with collection path
 * @return HTTP response with document array or error
 */
http_response_t* api_handle_documents_query(api_context_t* ctx, http_request_t* request);

/**
 * Handle specific document retrieval
 * 
 * Endpoint: GET /api/collections/{collection}/documents/{uuid}
 * Endpoint: GET /api/libraries/{library}/collections/{collection}/documents/{uuid}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with collection and document path
 * @return HTTP response with document or error
 */
http_response_t* api_handle_document_get(api_context_t* ctx, http_request_t* request);

/**
 * Handle document creation in collection
 * 
 * Endpoint: POST /api/collections/{collection}/documents
 * Endpoint: POST /api/libraries/{library}/collections/{collection}/documents
 * 
 * Features:
 * - Collection-to-type mapping
 * - Automatic field population
 * - Library and collection context
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with JSON document body
 * @return HTTP response with created document or error
 */
http_response_t* api_handle_document_create(api_context_t* ctx, http_request_t* request);

/**
 * Handle document update in collection
 * 
 * Endpoint: PUT /api/collections/{collection}/documents/{uuid}
 * Endpoint: PUT /api/libraries/{library}/collections/{collection}/documents/{uuid}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with JSON update body
 * @return HTTP response with updated document or error
 */
http_response_t* api_handle_document_update(api_context_t* ctx, http_request_t* request);

/**
 * Handle document deletion in collection
 * 
 * Endpoint: DELETE /api/collections/{collection}/documents/{uuid}
 * Endpoint: DELETE /api/libraries/{library}/collections/{collection}/documents/{uuid}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with document UUID
 * @return HTTP response with success confirmation or error
 */
http_response_t* api_handle_document_delete(api_context_t* ctx, http_request_t* request);

/**
 * ============================================================================
 * FIELD-LEVEL OPERATIONS API
 * ============================================================================
 * 
 * These functions handle field-level access and manipulation, supporting
 * granular document operations without full document loading.
 */

/**
 * Handle document field access
 * 
 * Endpoint: GET /api/collections/{collection}/documents/{uuid}/fields/{field}
 * 
 * Features:
 * - Nested field path support (e.g., "user.profile.email")
 * - Array index access (e.g., "items[0].price")
 * - Field projection for performance
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with field path
 * @return HTTP response with field value or error
 */
http_response_t* api_handle_document_field_access(api_context_t* ctx, http_request_t* request);

/**
 * Handle library-scoped document field access
 * 
 * Endpoint: GET /api/libraries/{library}/collections/{collection}/documents/{uuid}/fields/{field}
 * 
 * @param ctx API context with database and RBAC
 * @param request HTTP request with library and field path
 * @return HTTP response with field value or error
 */
http_response_t* api_handle_library_document_field_access(api_context_t* ctx, http_request_t* request);

/**
 * ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * Initialize document API module
 * 
 * @return true on success, false on failure
 */
bool api_documents_init(void);

/**
 * Cleanup document API module
 */
void api_documents_cleanup(void);

/**
 * Get document API route count
 * 
 * @return number of routes handled by this module
 */
size_t api_documents_get_route_count(void);

#endif /* API_DOCUMENTS_H */