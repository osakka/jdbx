#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_QUICKJS
#include "quickjs/quickjs.h"
#endif

int main(int argc, char** argv) {
    printf("Testing QuickJS integration\n");

#ifdef USE_QUICKJS
    // Initialize QuickJS
    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "Failed to create JS runtime\n");
        return 1;
    }
    
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "Failed to create JS context\n");
        JS_FreeRuntime(rt);
        return 1;
    }
    
    printf("QuickJS runtime initialized successfully\n");
    
    // Evaluate a simple JavaScript expression
    const char *expr = "({message: 'Hello, World!', sum: 2 + 2})";
    JSValue val = JS_Eval(ctx, expr, strlen(expr), "<input>", 0);
    
    if (JS_IsException(val)) {
        JSValue err = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, err);
        printf("JavaScript evaluation failed: %s\n", str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, err);
    } else {
        // Convert result to string and print it
        JSValue json = JS_JSONStringify(ctx, val, JS_NULL, JS_NULL);
        const char *str = JS_ToCString(ctx, json);
        printf("Result: %s\n", str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, json);
    }
    
    JS_FreeValue(ctx, val);
    
    // Clean up
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    printf("QuickJS test completed successfully\n");
#else
    printf("QuickJS is not available in this build\n");
#endif

    return 0;
}