#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

/* 
 * Debug macros - kept simple to avoid circular dependencies
 * For production code, use the LOG_* macros from logger.h directly
 * This is only for legacy compatibility
 */

/* Set DEBUG_MEMORY to 1 to enable debug prints, 0 to disable */
#define DEBUG_MEMORY 0

/* DEBUG_PRINT is disabled - use LOG_TRACE from logger.h for development debugging */
#define DEBUG_PRINT(fmt, ...) /* Disabled - use LOG_TRACE instead */


#endif /* DEBUG_H */