# ADR-001: JavaScript Integration with QuickJS

**Date**: May 1, 2025  
**Status**: Active  
**Deciders**: Technical Team  
**Technical Story**: Enable elegant database manipulation through JavaScript

## Context and Problem Statement

JDBX needed a way to provide expressive, high-level database operations while maintaining C-level performance. Traditional SQL or custom query languages add complexity and learning curves.

## Decision Drivers

- Developer experience and expressiveness
- Performance requirements (no external runtime)
- Binary size constraints
- Maintenance burden

## Considered Options

1. **Custom Query Language** - Create JDBX-specific query syntax
2. **SQL Integration** - Embed SQLite or similar
3. **JavaScript Engine** - Embed lightweight JS engine
4. **External Scripting** - Call external interpreters

## Decision Outcome

Chosen option: **JavaScript Engine (QuickJS)** because it provides:
- Familiar syntax for developers
- Small footprint (~500KB)
- No external dependencies
- Good performance characteristics
- Active maintenance

### Positive Consequences

- Elegant API for complex operations
- Validation and transformation functions
- No need to learn proprietary syntax
- Extensive ecosystem knowledge

### Negative Consequences

- Added binary size
- JavaScript parsing overhead
- Memory usage for JS context
- Security considerations for execution

## Implementation Details

```c
// JavaScript function registration
js_engine_register_function(engine, "validateUser", validate_user_js);
js_engine_register_function(engine, "transformDoc", transform_doc_js);
```

## Links

- [QuickJS Documentation](https://bellard.org/quickjs/)
- [JavaScript API Reference](../../reference/api/javascript.md)