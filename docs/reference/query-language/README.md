# Query Language Reference

Complete reference for the JDBX query language, operators, and functions.

## Contents

- **[Syntax](syntax.md)** - Query language syntax specification
- **[Operators](operators.md)** - Query operators reference *(Coming Soon)*
- **[Functions](functions.md)** - Built-in functions *(Coming Soon)*
- **[Examples](examples.md)** - Query examples *(Coming Soon)*

## Quick Reference

### Basic Query Structure
```json
{
  "filter": {
    "field": "value"
  },
  "sort": {
    "field": "asc"
  },
  "limit": 10
}
```

### Common Operators
- `$eq` - Equals
- `$ne` - Not equals  
- `$gt` - Greater than
- `$lt` - Less than
- `$in` - In array
- `$regex` - Regular expression

## See Also
- [API Documentation](../api/README.md)
- [Tutorial: Basic Queries](../../tutorials/beginner/basic-queries.md)