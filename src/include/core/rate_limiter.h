#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <time.h>
#include <stdint.h>
#include "database/database.h"
#include "utils/json.h"

// Rate limiter types stored as documents
#define DOC_TYPE_RATE_LIMIT "rate_limit"
#define DOC_TYPE_CIRCUIT_BREAKER "circuit_breaker"
#define DOC_TYPE_CONNECTION_RATE "connection_rate"

// Rate limiter configuration (stored in database)
typedef struct {
    int requests_per_minute;      // Max requests per minute per IP
    int burst_size;              // Token bucket burst size
    int connection_rate_limit;   // Max connections per second per IP
    int circuit_failure_threshold; // Failures before circuit opens
    int circuit_reset_timeout;   // Seconds before circuit half-opens
    int circuit_half_open_requests; // Requests allowed in half-open state
} rate_limiter_config_t;

// Rate limiter instance (uses database for storage)
typedef struct {
    database_t* db;              // Database for document storage
    rate_limiter_config_t config; // Current configuration
    time_t last_cleanup;         // Last cleanup timestamp
} rate_limiter_t;

// Circuit breaker states
typedef enum {
    CIRCUIT_CLOSED = 0,  // Normal operation
    CIRCUIT_OPEN = 1,    // Rejecting requests
    CIRCUIT_HALF_OPEN = 2 // Testing recovery
} circuit_state_t;

// Initialize rate limiter with database backend
rate_limiter_t* rate_limiter_init(database_t* db);
void rate_limiter_destroy(rate_limiter_t* limiter);

// Per-IP rate limiting
int rate_limiter_check_request(rate_limiter_t* limiter, const char* ip_address);
void rate_limiter_record_request(rate_limiter_t* limiter, const char* ip_address);

// Circuit breaker functions
circuit_state_t circuit_breaker_get_state(rate_limiter_t* limiter, const char* service_name);
void circuit_breaker_record_success(rate_limiter_t* limiter, const char* service_name);
void circuit_breaker_record_failure(rate_limiter_t* limiter, const char* service_name);

// Connection rate limiting
int connection_rate_check(rate_limiter_t* limiter, const char* ip_address);
void connection_rate_record(rate_limiter_t* limiter, const char* ip_address);

// Configuration management
void rate_limiter_reload_config(rate_limiter_t* limiter);
void rate_limiter_cleanup_expired(rate_limiter_t* limiter);
void rate_limiter_create_default_config(database_t* db);

#endif // RATE_LIMITER_H