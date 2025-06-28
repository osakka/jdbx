/**
 * @file rate_limiter.h
 * @brief Database-backed rate limiting system for JDBX
 * 
 * Implements comprehensive rate limiting using JDBX's unified documents
 * architecture for persistent state management. Provides multiple rate
 * limiting strategies including token bucket, circuit breaker, and
 * connection rate limiting.
 * 
 * Architecture:
 * - Database-backed persistence for rate limiting state
 * - Per-IP address token bucket algorithm implementation
 * - Circuit breaker pattern for service protection
 * - Connection-level rate limiting for DoS protection
 * 
 * Key Features:
 * - Thread-safe operations using mutex synchronization
 * - Automatic state cleanup and expiration handling
 * - Configurable limits and thresholds
 * - Integration with JDBX unified documents storage
 * 
 * @performance O(1) rate checking with database persistence overhead
 * @threadsafe All operations protected by internal mutex
 * @memory Minimal memory footprint - state stored in database
 */

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include "database/database.h"
#include "utils/json.h"

/**
 * Document types for rate limiter state storage
 * 
 * Used as document type identifiers in the unified documents collection
 * for organizing different aspects of rate limiting state.
 */
#define DOC_TYPE_RATE_LIMIT "rate_limit"         /** Per-IP request rate limiting state */
#define DOC_TYPE_CIRCUIT_BREAKER "circuit_breaker" /** Circuit breaker service state */
#define DOC_TYPE_CONNECTION_RATE "connection_rate" /** Per-IP connection rate state */

/**
 * Rate limiter configuration structure
 * 
 * Defines all rate limiting parameters for the system. Configuration
 * is persisted in the database and can be modified at runtime.
 * 
 * @note Configuration changes require rate limiter restart to take effect
 * @memory Configuration persisted in database, structure is stack-allocated
 */
typedef struct {
    /** Maximum requests per minute per IP address (range: 1-10000) */
    int requests_per_minute;
    
    /** Token bucket burst size allowing temporary rate spikes (range: 1-100) */
    int burst_size;
    
    /** Maximum new connections per second per IP address (range: 1-1000) */
    int connection_rate_limit;
    
    /** Number of failures before circuit breaker opens (range: 1-100) */
    int circuit_failure_threshold;
    
    /** Seconds before circuit breaker transitions to half-open (range: 1-3600) */
    int circuit_reset_timeout;
    
    /** Number of test requests allowed in half-open state (range: 1-10) */
    int circuit_half_open_requests;
} rate_limiter_config_t;

/**
 * Rate limiter instance structure
 * 
 * Represents an active rate limiter with database-backed persistence.
 * All rate limiting state is stored in the associated database using
 * the unified documents architecture.
 * 
 * @note Must be initialized with rate_limiter_init() before use
 * @threadsafe All operations are protected by internal mutex
 * @memory Minimal memory footprint - state persisted in database
 */
typedef struct {
    /** Thread safety mutex - protects all rate limiter operations */
    pthread_mutex_t mutex;
    
    /** Database instance for persistent state storage */
    database_t* db;
    
    /** Current rate limiting configuration parameters */
    rate_limiter_config_t config;
    
    /** Timestamp of last cleanup operation for expired entries */
    time_t last_cleanup;
} rate_limiter_t;

/**
 * Circuit breaker states
 * 
 * Defines the three states of the circuit breaker pattern used for
 * service protection and failure handling.
 */
typedef enum {
    /** Normal operation - requests pass through */
    CIRCUIT_CLOSED = 0,
    
    /** Failure threshold exceeded - rejecting all requests */
    CIRCUIT_OPEN = 1,
    
    /** Testing recovery - allowing limited requests */
    CIRCUIT_HALF_OPEN = 2
} circuit_state_t;

/*
 * ============================================================================
 * Rate Limiter Lifecycle Management
 * ============================================================================
 */

/**
 * Initialize rate limiter with database backend
 * 
 * Creates a new rate limiter instance using the provided database for
 * persistent state storage. Loads configuration and initializes internal
 * structures for thread-safe operation.
 * 
 * @param db Database instance for state persistence (must be initialized)
 * @return New rate limiter instance or NULL on failure
 * 
 * @note Caller responsible for calling rate_limiter_destroy() on the result
 * @threadsafe Initialization is not thread-safe - use from single thread
 * @memory Allocates rate limiter structure - freed by rate_limiter_destroy()
 */
rate_limiter_t* rate_limiter_init(database_t* db);

/**
 * Destroy rate limiter and free resources
 * 
 * Cleans up rate limiter instance, destroys mutex, and frees allocated memory.
 * Does not affect persistent state stored in the database.
 * 
 * @param limiter Rate limiter instance to destroy (may be NULL)
 * 
 * @note Safe to call with NULL pointer
 * @threadsafe Not thread-safe - ensure no concurrent access
 * @memory Frees all memory associated with the rate limiter
 */
void rate_limiter_destroy(rate_limiter_t* limiter);

/*
 * ============================================================================
 * Per-IP Request Rate Limiting
 * ============================================================================
 */

/**
 * Check if request is allowed for IP address
 * 
 * Implements token bucket algorithm to determine if a request from the
 * specified IP address should be allowed based on current rate limits.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param ip_address IP address to check (null-terminated string)
 * @return 1 if request allowed, 0 if rate limited
 * 
 * @note Does not modify state - use rate_limiter_record_request() to consume tokens
 * @performance O(1) with database lookup overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory No additional memory allocation
 */
int rate_limiter_check_request(rate_limiter_t* limiter, const char* ip_address);

/**
 * Record request and update rate limiting state
 * 
 * Updates the token bucket for the specified IP address, consuming tokens
 * and updating the database state for future rate limiting decisions.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param ip_address IP address making the request (null-terminated string)
 * 
 * @note Should be called after rate_limiter_check_request() returns 1
 * @performance O(1) with database update overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory Updates database state - minimal memory allocation
 */
void rate_limiter_record_request(rate_limiter_t* limiter, const char* ip_address);

/*
 * ============================================================================
 * Circuit Breaker Pattern Implementation
 * ============================================================================
 */

/**
 * Get current circuit breaker state for service
 * 
 * Returns the current state of the circuit breaker for the specified service,
 * used to determine if requests should be allowed or rejected.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param service_name Service name identifier (null-terminated string)
 * @return Current circuit breaker state (CLOSED/OPEN/HALF_OPEN)
 * 
 * @note State transitions happen automatically based on failure thresholds
 * @performance O(1) with database lookup overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory No additional memory allocation
 */
circuit_state_t circuit_breaker_get_state(rate_limiter_t* limiter, const char* service_name);

/**
 * Record successful request for circuit breaker
 * 
 * Updates circuit breaker state to reflect a successful request, potentially
 * transitioning from HALF_OPEN to CLOSED state if thresholds are met.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param service_name Service name identifier (null-terminated string)
 * 
 * @note Call this for every successful service request
 * @performance O(1) with database update overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory Updates database state - minimal memory allocation
 */
void circuit_breaker_record_success(rate_limiter_t* limiter, const char* service_name);

/**
 * Record failed request for circuit breaker
 * 
 * Updates circuit breaker state to reflect a failed request, potentially
 * transitioning from CLOSED to OPEN state if failure threshold is exceeded.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param service_name Service name identifier (null-terminated string)
 * 
 * @note Call this for every failed service request
 * @performance O(1) with database update overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory Updates database state - minimal memory allocation
 */
void circuit_breaker_record_failure(rate_limiter_t* limiter, const char* service_name);

/*
 * ============================================================================
 * Connection Rate Limiting
 * ============================================================================
 */

/**
 * Check if new connection is allowed for IP address
 * 
 * Implements connection-level rate limiting to prevent connection flooding
 * attacks. Tracks new connections per second per IP address.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param ip_address IP address attempting connection (null-terminated string)
 * @return 1 if connection allowed, 0 if rate limited
 * 
 * @note Use for new connection attempts, not existing connections
 * @performance O(1) with database lookup overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory No additional memory allocation
 */
int connection_rate_check(rate_limiter_t* limiter, const char* ip_address);

/**
 * Record new connection from IP address
 * 
 * Updates connection rate limiting state for the specified IP address,
 * incrementing connection counters for future rate limiting decisions.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * @param ip_address IP address that established connection (null-terminated string)
 * 
 * @note Call after successful connection establishment
 * @performance O(1) with database update overhead
 * @threadsafe Thread-safe using internal mutex
 * @memory Updates database state - minimal memory allocation
 */
void connection_rate_record(rate_limiter_t* limiter, const char* ip_address);

/*
 * ============================================================================
 * Configuration and Maintenance
 * ============================================================================
 */

/**
 * Reload configuration from database
 * 
 * Refreshes rate limiter configuration by reading current settings from
 * the database. Used to apply configuration changes at runtime.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * 
 * @note Configuration changes take effect immediately
 * @performance O(1) database lookup
 * @threadsafe Thread-safe using internal mutex
 * @memory No additional memory allocation
 */
void rate_limiter_reload_config(rate_limiter_t* limiter);

/**
 * Clean up expired rate limiting entries
 * 
 * Removes expired entries from the database to prevent unbounded growth
 * of rate limiting state. Should be called periodically.
 * 
 * @param limiter Rate limiter instance (must be initialized)
 * 
 * @note Cleanup is automatically triggered during normal operations
 * @performance O(n) where n is number of expired entries
 * @threadsafe Thread-safe using internal mutex
 * @memory Frees memory by removing expired database entries
 */
void rate_limiter_cleanup_expired(rate_limiter_t* limiter);

/**
 * Create default configuration in database
 * 
 * Initializes the database with default rate limiting configuration
 * if no configuration exists. Safe to call multiple times.
 * 
 * @param db Database instance for storing configuration (must be initialized)
 * 
 * @note Only creates configuration if none exists
 * @performance O(1) database operations
 * @threadsafe Thread-safe with respect to database operations
 * @memory Creates configuration document in database
 */
void rate_limiter_create_default_config(database_t* db);

#endif // RATE_LIMITER_H