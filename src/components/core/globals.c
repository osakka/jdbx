/**
 * @file globals.c
 * @brief Global variable definitions for JDBX core components
 * 
 * Provides centralized storage for singleton instances and global state
 * required across multiple modules. Minimizes global scope to essential
 * components only, maintaining clear ownership and lifecycle management.
 */

#include "core/rate_limiter.h"
#include <stddef.h>

/**
 * Global rate limiter instance
 * 
 * Singleton rate limiter providing system-wide request throttling.
 * Initialized during server startup in main.c, accessed by request
 * handlers for connection and API rate limiting enforcement.
 * 
 * Lifecycle:
 * - Created: During server initialization
 * - Access: Read-only access from request handlers
 * - Destroyed: During server shutdown
 */
rate_limiter_t* g_rate_limiter = NULL;