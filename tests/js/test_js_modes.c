#include <stdio.h>
#include "jsondb/core/server.h"  /* For http types */
#include "jsondb/api/api.h"      /* For API context types */
#include "jsondb/js/js_engine.h"
#include "jsondb/js/js_api.h"
#include "jsondb/database/database.h"

int main() {
    printf("Testing JavaScript conditional compilation...\n");

#ifndef DISABLE_JS
    printf("JavaScript is ENABLED in this build.\n");

    // Test JavaScript initialization
    database_t *db = NULL;  // Just for testing, normally would be initialized

    // Initialize JavaScript
    js_api_init(db);

    // Try to evaluate a simple script (will fail due to null engine, but tests compilation)
    char *result = NULL;
    js_engine_eval(g_js_engine, "1+1", &result);

    // Clean up
    js_api_cleanup();

    printf("Successfully compiled and called JavaScript functions.\n");
#else
    printf("JavaScript is DISABLED in this build.\n");

    // Test stub functions
    database_t *db = NULL;  // Just for testing

    // These should be no-ops when DISABLE_JS is defined
    js_api_init(db);
    js_api_cleanup();

    printf("Successfully called JavaScript stub functions.\n");
#endif

    return 0;
}
