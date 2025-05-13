#include "src/include/core/server.h"
#include "src/include/database/database.h"
#include "src/include/rbac/rbac.h"
#include "src/include/api/api.h"
#include "src/include/rbac/jwt.h"
#include "src/include/utils/metrics.h"

/* JavaScript dependencies */
#ifndef DISABLE_JS
#include "src/include/js/js_api.h"
#include "src/include/js/js_engine.h"
#else
/* Stub definitions when JavaScript is disabled */
typedef void js_engine_t;
void js_api_init(database_t* db) { 
    /* No-op implementation when JS is disabled */
    (void)db; /* Avoid unused parameter warning */
}
void js_api_cleanup() { 
    /* No-op implementation when JS is disabled */
}
int js_execute_file(js_engine_t* engine, const char* filename) {
    /* No-op implementation when JS is disabled */
    (void)engine; /* Avoid unused parameter warning */
    (void)filename; /* Avoid unused parameter warning */
    return 0;
}
void js_engine_free(js_engine_t* engine) {
    /* No-op implementation when JS is disabled */
    (void)engine; /* Avoid unused parameter warning */
}
js_engine_t* g_js_engine = NULL;
#endif

#include <stdio.h>

int main() {
    printf("JavaScript Compilation Test\n");
    printf("==========================\n\n");

#ifndef DISABLE_JS
    printf("JavaScript is ENABLED in this build.\n");
    printf("Can access js_api_init() and other JavaScript functions.\n");
#else
    printf("JavaScript is DISABLED in this build.\n");
    printf("Using stub implementations of JavaScript functions.\n");
#endif

    /* Test the conditional compilation by calling a JavaScript function */
    database_t* dummy_db = NULL;
    js_api_init(dummy_db);  /* This should work with either compilation mode */

    printf("\nTest completed successfully!\n");
    return 0;
}