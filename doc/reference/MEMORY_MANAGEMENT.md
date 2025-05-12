# Memory Management in JSON Database Server

This document describes the memory management system used in the JSON Database Server project, focusing on the reference counting mechanism implemented to prevent double-free errors and memory leaks.

## Overview

The JSON Database Server uses a reference counting system to manage shared objects, particularly when integrating with the QuickJS JavaScript engine. This approach ensures that objects are only freed when no component is using them anymore, preventing memory leaks and double-free errors.

## Reference Counting System

The reference counting system consists of three main components:

1. **Core Reference Counter**: A generic reference counting mechanism for any type of object
2. **JSON Reference Counter**: A specialized reference counting system for JSON values
3. **RBAC Reference Counter**: A specialized reference counting system for the RBAC (Role-Based Access Control) system

### Core Reference Counter

The core reference counter is defined in `include/utils/memory/ref_counter.h` and implemented in `src/utils/memory/ref_counter.c`. It provides the following functionality:

- Create a reference counted object with an initial reference count of 1
- Acquire a reference to an object (increment the reference count)
- Release a reference to an object (decrement the reference count)
- Get the current reference count
- Get the object pointer
- Automatically free the object when the reference count reaches zero

```c
// Create a reference counted object
ref_counted_t* rc = ref_counter_create(my_object, my_free_function);

// Acquire a reference (increment reference count)
ref_counter_acquire(rc);

// Release a reference (decrement reference count, free if zero)
ref_counter_release(rc);

// Get the object
void* obj = ref_counter_get(rc);
```

### JSON Reference Counter

The JSON reference counter is built on top of the core reference counter and is specialized for JSON values. It's defined in `include/utils/memory/ref_json.h` and implemented in `src/utils/memory/ref_json.c`. It provides:

- Create a reference counted JSON value
- Get a JSON value from a reference counted wrapper
- Acquire a reference to a JSON value
- Release a reference to a JSON value
- A registry of all reference counted JSON values to prevent double-wrapping

```c
// Create a reference counted JSON value
ref_counted_t* rc = ref_json_create(json_value);

// Get the JSON value
json_value_t* json = ref_json_get(rc);

// Acquire a reference (creates a new wrapper if needed)
ref_counted_t* rc2 = ref_json_acquire(json_value);

// Release a reference
ref_json_release(rc);
```

### RBAC Reference Counter

The RBAC reference counter is a specialized system for the RBAC system, which uses JSON objects to store users and roles. It's defined in `include/rbac_refcount.h` and implemented in `src/rbac_refcount.c`. It provides:

- Create a reference counted RBAC system
- Free a reference counted RBAC system
- Save and load a reference counted RBAC system
- Get the underlying JSON objects
- Convert between regular RBAC and reference counted RBAC

```c
// Create a reference counted RBAC system
rbac_refcount_t* rbac = rbac_refcount_init();

// Free the RBAC system
rbac_refcount_free(rbac);

// Save the RBAC system
rbac_refcount_save(rbac, path);

// Load the RBAC system
rbac_refcount_t* rbac = rbac_refcount_load(path);

// Get the underlying JSON objects
json_value_t* users = rbac_refcount_get_users(rbac);
json_value_t* roles = rbac_refcount_get_roles(rbac);
```

## JavaScript Integration

The JavaScript engine integration uses the reference counting system to manage JSON values that are shared between C and JavaScript. This is implemented in:

- `include/js/js_engine.h`
- `src/js/js_engine.c`

The key functions for this integration are:

- `json_to_js_value`: Convert a JSON value to a JavaScript value
- `js_value_to_json`: Convert a JavaScript value to a JSON value
- `ref_json_to_js_value`: Convert a reference counted JSON value to a JavaScript value
- `js_value_to_ref_json`: Convert a JavaScript value to a reference counted JSON value

## Cleanup Process

The cleanup process is carefully ordered to prevent double-free issues:

1. Save any persistent data first
2. Clean up the JavaScript engine, which might use the database and RBAC
3. Clean up the API context, which might use the database and RBAC
4. Clean up metrics
5. Clean up the database
6. Clean up the server configuration
7. Finally clean up the RBAC system

This order ensures that components that depend on other components are cleaned up first.

## Best Practices

When working with the JSON Database Server codebase, follow these best practices:

1. **Use reference counting for shared objects**: Any object that might be shared between components should use reference counting.
2. **Clear pointers after freeing**: Always set pointers to NULL after freeing them to prevent use-after-free issues.
3. **Check for NULL before dereferencing**: Always check if a pointer is NULL before dereferencing it.
4. **Free in the correct order**: Free objects in the reverse order of their creation to prevent use-after-free issues.
5. **Avoid circular references**: Circular references can lead to memory leaks even with reference counting.

## Debugging Memory Issues

If you encounter memory issues, use these techniques:

1. **Valgrind**: Use Valgrind's Memcheck tool to detect memory leaks and use-after-free issues.
2. **AddressSanitizer**: Use AddressSanitizer to detect memory issues at runtime.
3. **Debug logging**: Add debug logging to track reference counts and memory allocation/deallocation.
4. **Memory breakpoints**: Use a debugger to set breakpoints at memory allocation/deallocation points.

## Double-Free Prevention

A critical improvement to the memory management system is the implementation of a double-free prevention mechanism in the JSON value handling. This is essential when we have shared objects like strings that might be referenced by multiple JSON objects.

The solution includes:

1. **Address Tracking**: A registry that tracks addresses of all freed JSON values to prevent double-freeing.
2. **Safe Freeing Process**: A modified JSON free function that checks if a value has been freed before attempting to free it again.
3. **Nullification of Pointers**: All pointers are nullified after freeing to prevent use-after-free issues.
4. **Debug Logging**: Optional debug prints that show reference counting operations and prevented double-free attempts.

Example of the double-free prevention logic:

```c
void json_free(json_value_t* value) {
    if (!value) {
        return;
    }
    
    /* Check if this object was already freed */
    if (was_freed(value)) {
        printf("DEBUG: Preventing double-free of JSON value %p\n", (void*)value);
        return;
    }
    
    /* Mark this address as freed */
    add_to_freed(value);
    
    /* Continue with normal freeing process */
    /* ... */
}
```

## Further Improvements

Future improvements to the memory management system might include:

1. **Automatic Garbage Collection**: Implement a mark-and-sweep garbage collector to automatically detect and free unreachable objects.
2. **Memory Pooling**: Implement a memory pool for frequently allocated objects of the same size.
3. **Memory Usage Tracking**: Implement tracking of memory usage for performance monitoring.
4. **Thread-Safe Reference Counting**: Make the reference counting system thread-safe for multi-threaded use.
5. **Hash-Based Registry**: Replace the fixed-size array registries with hash tables for better performance with large numbers of objects.
6. **Automatic Cycle Detection**: Add automatic detection of circular references to prevent memory leaks.

By following these guidelines and using the reference counting system with double-free prevention, we can prevent memory issues and ensure the stability of the JSON Database Server.