#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/include/quickjs/quickjs.h"

/**
 * Simple standalone QuickJS test program
 * This verifies that the QuickJS library is properly linked
 * without depending on the full server implementation
 */

/* Simple evaluation function */
static char* evaluate_js(JSContext *ctx, const char* script) {
    JSValue val = JS_Eval(ctx, script, strlen(script), "<input>", 0);
    
    char* result = NULL;
    
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exception);
        if (str) {
            printf("Exception: %s\n", str);
            result = strdup(str);
            JS_FreeCString(ctx, str);
        } else {
            result = strdup("Unknown exception");
        }
        JS_FreeValue(ctx, exception);
    } else {
        JSValue json_val = JS_JSONStringify(ctx, val, JS_NULL, JS_NULL);
        
        if (JS_IsException(json_val)) {
            const char* str = JS_ToCString(ctx, val);
            if (str) {
                result = strdup(str);
                JS_FreeCString(ctx, str);
            } else {
                result = strdup("");
            }
        } else {
            const char* str = JS_ToCString(ctx, json_val);
            if (str) {
                result = strdup(str);
                JS_FreeCString(ctx, str);
            } else {
                result = strdup("");
            }
            JS_FreeValue(ctx, json_val);
        }
    }
    
    JS_FreeValue(ctx, val);
    return result;
}

int main(int argc, char** argv) {
    printf("QuickJS Standalone Test\n");
    printf("======================\n\n");
    
    /* Create a new JavaScript runtime */
    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        fprintf(stderr, "Failed to create JavaScript runtime\n");
        return 1;
    }
    
    /* Create a new JavaScript context */
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        fprintf(stderr, "Failed to create JavaScript context\n");
        JS_FreeRuntime(rt);
        return 1;
    }
    
    /* Test basic expression evaluation */
    printf("Testing basic expression: 2 + 3 * 4\n");
    char* result1 = evaluate_js(ctx, "2 + 3 * 4");
    printf("Result: %s\n\n", result1);
    free(result1);
    
    /* Test object creation and manipulation */
    printf("Testing object creation and manipulation\n");
    const char* object_test = 
        "const obj = { name: 'Test Object', value: 42 };\n"
        "obj.newProperty = 'added property';\n"
        "obj.value *= 2;\n"
        "obj";
    
    char* result2 = evaluate_js(ctx, object_test);
    printf("Result: %s\n\n", result2);
    free(result2);
    
    /* Test JSON handling */
    printf("Testing JSON handling\n");
    const char* json_test =
        "const data = { a: 1, b: [2, 3, 4], c: { nested: true } };\n"
        "const json = JSON.stringify(data);\n"
        "const parsed = JSON.parse(json);\n"
        "parsed.verification = 'JSON round-trip successful';\n"
        "parsed";
    
    char* result3 = evaluate_js(ctx, json_test);
    printf("Result: %s\n\n", result3);
    free(result3);
    
    /* Test error handling */
    printf("Testing error handling\n");
    char* result4 = evaluate_js(ctx, "nonExistentVariable + 1");
    printf("Result: %s\n\n", result4);
    free(result4);
    
    /* Clean up */
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    printf("Test completed successfully\n");
    return 0;
}