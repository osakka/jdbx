# ADR-001: JavaScript Integration with QuickJS

**Date**: May 1, 2025  
**Status**: Accepted  
**Version**: 1.0.0  
**Impact**: High  

## Context

JDBX needed a way to provide expressive, high-level database operations while maintaining C-level performance. Traditional SQL or custom query languages add complexity and learning curves.

## Decision

Integrate QuickJS JavaScript engine to enable:
- Document validators written in JavaScript
- Document transformers for data manipulation
- Custom business functions
- Native storage API bindings

## Rationale

### Developer Experience
- Familiar JavaScript syntax
- No proprietary query language to learn
- Extensive ecosystem knowledge
- Rapid development of business logic

### Technical Benefits
- Small footprint (~500KB)
- No external dependencies
- Good performance characteristics
- Active maintenance by Fabrice Bellard

### Integration Points
- Validators run before document insert/update
- Transformers modify documents in pipeline
- Functions callable via REST API
- Native bindings for storage access

## Implementation

### JavaScript Function Types

1. **Validators**
```javascript
function validateUser(doc) {
  if (!doc.email || !doc.email.includes('@')) {
    addError('email', 'Invalid email address');
  }
  return isValid;
}
```

2. **Transformers**
```javascript
function transformUser(doc, operation) {
  if (operation === 'insert') {
    doc.created_at = new Date().toISOString();
  }
  doc.updated_at = new Date().toISOString();
  return doc;
}
```

3. **Custom Functions**
```javascript
function calculateMetrics(args) {
  const startTime = performance.now();
  // Business logic
  logMetric('calculateMetrics', performance.now() - startTime);
  return result;
}
```

### Native API Bindings
- `storage.insert(doc)`
- `storage.query(filter)`
- `storage.update(id, doc)`
- `storage.delete(id)`

## Consequences

### Positive
- **Elegant API**: Complex operations in familiar syntax
- **Rapid Development**: Business logic without recompilation
- **Performance**: JIT compilation for hot paths
- **Ecosystem**: JavaScript knowledge widely available

### Negative
- **Binary Size**: Added ~500KB to executable
- **Memory Usage**: JS context overhead per thread
- **Security**: Script execution requires sandboxing
- **Complexity**: Additional subsystem to maintain

### Mitigations
- Conditional compilation flag to exclude JS
- Memory limits on JS execution contexts
- Sandboxed execution environment
- Comprehensive test coverage

## Technical Details

### Files Created
- `src/components/js/js_engine.c` - QuickJS integration
- `src/components/js/js_api.c` - Native bindings
- `src/components/js/js_native_storage.c` - Storage API
- `src/include/js/js_engine.h` - Public interface

### Configuration
- `DISABLE_JS` compile flag for non-JS builds
- `--js-memory-limit` runtime configuration
- `--js-timeout` execution timeout

## Validation

- ✅ QuickJS integrated successfully
- ✅ All three function types working
- ✅ Native API bindings functional
- ✅ Performance within requirements
- ✅ Conditional compilation working

## References

- QuickJS: https://bellard.org/quickjs/
- Git commit: `2bab076` - Initial JavaScript integration
- Related: ADR-002 (Performance Architecture)