# JSON String Handling Guidelines

## Overview

This document provides guidelines for consistent JSON string handling throughout the codebase. Following these guidelines ensures memory safety, consistent error handling, and improved maintainability.

## Core Principles

1. **Use helper functions consistently** - Always use the helper functions in `include/utils/json_helpers.h` rather than direct JSON API calls
2. **Clear ownership semantics** - Be explicit about who owns memory and when it should be freed
3. **Consistent error handling** - Use standardized error messages and response formats
4. **Validation before usage** - Always validate JSON input before processing

## Helper Functions

### Request Parsing

```c
/* Parse request body */
json_value_t* body = json_parse_request_body(request);
if (!body) {
    return http_response_error("Invalid JSON body", 400);
}

/* Validate required fields */
const char* required_fields[] = {"field1", "field2"};
const char* missing_field = NULL;
if (!json_validate_required_fields(body, required_fields, 2, &missing_field)) {
    char error_msg[100];
    sprintf(error_msg, "Missing required field: %s", missing_field);
    json_free(body);
    return http_response_error(error_msg, 400);
}

/* Get values with validation */
const char* str_value = json_get_string_value(body, "str_field", 1);  /* Required */
int int_value = json_get_int_value(body, "int_field", 0);            /* Optional with default */
```

### Response Creation

```c
/* Success response with data */
json_value_t* data = json_create_object();
json_object_set(data, "result", json_create_string("success"));
/* ... add more fields ... */
return http_response_success_with_data("Operation successful", data, 200);
/* Note: data is owned and freed by the function */

/* Detailed error response */
return http_response_error_detailed("Item not found", "ITEM_NOT_FOUND", 404);

/* From JSON string (taking ownership) */
char* json_str = json_stringify(json_obj);
json_free(json_obj);  /* Free the JSON object as we don't need it anymore */
return http_response_from_json_string(json_str, 200);  /* json_str will be freed */

/* From constant JSON string (not taking ownership) */
return http_response_json_string("{\"status\":\"success\"}", 200);
```

## Memory Ownership Rules

1. **Taking Ownership**
   - Functions with names like `xxx_from_yyy` typically take ownership of their inputs
   - Example: `http_response_from_json_string()` takes ownership of the string and frees it

2. **Not Taking Ownership**
   - Functions without "from" typically do not take ownership
   - Example: `http_response_json_string()` does not take ownership of the string

3. **Giving Ownership**
   - Functions that provide data typically give ownership to the caller
   - Example: `json_stringify()` gives ownership of the returned string to the caller

4. **Function Arguments**
   - When passing a value to a function like `json_object_set()` or `json_array_append()`, ownership is transferred
   - The container (object or array) takes ownership of the value

## Error Handling Standards

1. **JSON Parsing Errors**
   - Always check for NULL after calling `json_parse_*()`
   - Use status code 400 (Bad Request) for invalid JSON input

2. **Missing Fields**
   - Include the missing field name in the error message
   - Use status code 400 (Bad Request) for missing required fields

3. **Type Errors**
   - When a field has the wrong type, provide a clear error message
   - Use status code 400 (Bad Request) for type errors

4. **Server Errors**
   - Use status code 500 (Internal Server Error) for memory allocation failures
   - Include a generic message that doesn't expose internal details

## Response Format Standards

1. **Success Responses**
   ```json
   {
     "status": "success",
     "message": "Operation completed successfully",
     "data": {
       "id": "123",
       "name": "Example"
     }
   }
   ```

2. **Error Responses**
   ```json
   {
     "status": "error",
     "message": "User-friendly error message",
     "code": "ERROR_CODE"
   }
   ```

## Code Examples

### Good Example - API Handler

```c
http_response_t* api_handle_get_item(api_context_t* ctx, http_request_t* request) {
    if (!ctx || !request) {
        return http_response_error("Invalid request", 400);
    }
    
    /* Extract item ID from path */
    const char* id = extract_id_from_path(request->path);
    if (!id) {
        return http_response_error_detailed("Missing item ID", "MISSING_ID", 400);
    }
    
    /* Get item from database */
    json_value_t* item = db_get_item(ctx->db, id);
    if (!item) {
        return http_response_error_detailed("Item not found", "ITEM_NOT_FOUND", 404);
    }
    
    /* Return success response */
    return http_response_success_with_data("Item retrieved successfully", item, 200);
}
```

### Bad Example - Avoid This

```c
http_response_t* api_handle_get_item(api_context_t* ctx, http_request_t* request) {
    /* Direct string manipulation - error prone */
    char* json_str = malloc(1024);
    sprintf(json_str, "{\"id\":\"%s\",\"name\":\"%s\"}", item->id, item->name);
    
    /* Manual response creation - inconsistent */
    http_response_t* response = create_http_response(200, json_str, "application/json");
    
    /* Memory leak - doesn't free json_str */
    return response;
}
```

## Performance Considerations

1. **Avoid Unnecessary Conversions**
   - Don't convert JSON to string and back unnecessarily
   - Use the appropriate helper functions to avoid extra conversions

2. **Minimize Memory Allocations**
   - Reuse JSON objects when possible
   - Free unused objects as soon as they're no longer needed

3. **Validation Efficiency**
   - Only validate what's necessary for your logic
   - For large objects, consider selective validation

## Implementation Checklist

When implementing JSON handling:

- [ ] Use `json_parse_request_body()` for parsing request bodies
- [ ] Validate required fields with `json_validate_required_fields()`
- [ ] Get values using type-specific getters (`json_get_string_value()`, etc.)
- [ ] Create responses using the appropriate response helper
- [ ] Follow consistent memory ownership rules
- [ ] Free JSON objects when you're done with them
- [ ] Use consistent error handling patterns
- [ ] Follow the standard response format