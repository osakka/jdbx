#ifndef QUICKJS_H
#define QUICKJS_H

#include <stddef.h>
#include <stdint.h>

/* Basic types for QuickJS mock implementation */
typedef struct JSContext JSContext;
typedef struct JSRuntime JSRuntime;
typedef struct JSValue {
    int64_t tag;
    union {
        int32_t int32;
        int64_t int64;
        double float64;
        void *ptr;
    } u;
} JSValue;
typedef struct JSObject JSObject;
typedef struct JSProperty JSProperty;
typedef struct JSClass JSClass;
typedef struct JSClassID {
    uint32_t id;
} JSClassID;
typedef uint32_t JSAtom;
typedef JSValue JSValueConst;

/* Property enumeration structure */
typedef struct JSPropertyEnum {
    JSAtom atom;
    uint32_t is_enumerable;
} JSPropertyEnum;

/* Flags for JS_GPN */
#define JS_GPN_STRING_MASK  (1 << 0)

/* ValueConst for function arguments */
#define JS_EVAL_TYPE_GLOBAL 0

/* Predefined values */
extern JSValue JS_EXCEPTION;
extern JSValue JS_UNDEFINED;
extern JSValue JS_NULL;
extern JSValue JS_FALSE;
extern JSValue JS_TRUE;

/* Runtime management */
JSRuntime *JS_NewRuntime(void);
void JS_FreeRuntime(JSRuntime *rt);
JSContext *JS_NewContext(JSRuntime *rt);
void JS_FreeContext(JSContext *ctx);
void JS_SetContextOpaque(JSContext *ctx, void *opaque);
void *JS_GetContextOpaque(JSContext *ctx);
JSValue JS_GetGlobalObject(JSContext *ctx);

/* Object creation */
JSValue JS_NewObject(JSContext *ctx);
JSValue JS_NewString(JSContext *ctx, const char *str);
JSValue JS_NewStringLen(JSContext *ctx, const char *str, size_t len);
JSValue JS_NewInt32(JSContext *ctx, int32_t val);
JSValue JS_NewInt64(JSContext *ctx, int64_t val);
JSValue JS_NewFloat64(JSContext *ctx, double val);
JSValue JS_NewBool(JSContext *ctx, int val);
JSValue JS_NewArray(JSContext *ctx);

/* Function creation */
JSValue JS_NewCFunction(JSContext *ctx, void *func, const char *name, int length);
JSValue JS_NewCFunction2(JSContext *ctx, void *func, const char *name, int length, int cproto, int magic);

/* Property functions */
int JS_SetPropertyStr(JSContext *ctx, JSValue this_obj, const char *prop, JSValue val);
int JS_SetPropertyUint32(JSContext *ctx, JSValue this_obj, uint32_t idx, JSValue val);
JSValue JS_GetPropertyStr(JSContext *ctx, JSValue this_obj, const char *prop);
JSValue JS_GetPropertyUint32(JSContext *ctx, JSValue this_obj, uint32_t idx);
JSValue JS_GetProperty(JSContext *ctx, JSValue this_obj, JSAtom prop);
JSValue JS_GetPropertyInternal(JSContext *ctx, JSValue obj, JSAtom prop, JSValue receiver, int throw_ref_error);
int JS_GetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen, JSValue obj, int flags);
JSValue JS_AtomToString(JSContext *ctx, JSAtom atom);
void JS_FreeAtom(JSContext *ctx, JSAtom atom);

/* Value management */
void JS_FreeValue(JSContext *ctx, JSValue val);
const char* JS_ToCString(JSContext *ctx, JSValue val);
void JS_FreeCString(JSContext *ctx, const char *ptr);
JSValue JS_GetException(JSContext *ctx);
void __JS_FreeValue(JSContext *ctx, JSValue v);
JSValue JS_DupValue(JSContext *ctx, JSValue val);

/* Type checking functions */
int JS_IsException(JSValue val);
int JS_IsUndefined(JSValue val);
int JS_IsNull(JSValue val);
int JS_IsBool(JSValue val);
int JS_IsNumber(JSValue val);
int JS_IsString(JSValue val);
int JS_IsObject(JSValue val);
int JS_IsArray(JSContext *ctx, JSValue val);

/* Opaque pointer handling */
void JS_SetOpaque(JSValue obj, void *opaque);
void *JS_GetOpaque(JSValue obj, JSClassID class_id);

/* Type conversion */
int JS_ToFloat64(JSContext *ctx, double *pres, JSValue val);
int JS_ToInt64(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToBool(JSContext *ctx, JSValue val);
const char *JS_ToCStringLen2(JSContext *ctx, size_t *plen, JSValue val, int cesu8);

/* JavaScript execution */
JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len, const char *filename, int eval_flags);

/* Error handling */
JSValue JS_ThrowTypeError(JSContext *ctx, const char *fmt, ...);
JSValue JS_ThrowInternalError(JSContext *ctx, const char *fmt, ...);

/* JSON functions */
JSValue JS_JSONStringify(JSContext *ctx, JSValue val, JSValue replacer, JSValue space);

/* Memory management */
void js_free(JSContext *ctx, void *ptr);

#endif /* QUICKJS_H */