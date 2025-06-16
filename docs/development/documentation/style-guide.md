# JDBX Documentation Style Guide

> The definitive guide for writing clear, consistent, and maintainable documentation for JDBX

## Table of Contents

- [Overview](#overview)
- [Voice and Tone](#voice-and-tone)
- [File Naming](#file-naming)
- [Document Structure](#document-structure)
- [Markdown Standards](#markdown-standards)
- [Code Examples](#code-examples)
- [API Documentation](#api-documentation)
- [Cross-References](#cross-references)
- [Versioning](#versioning)
- [Maintenance](#maintenance)

## Overview

This style guide ensures consistency across all JDBX documentation. It covers naming conventions, formatting standards, and content guidelines that all contributors should follow.

## Voice and Tone

### Writing Principles

1. **Clear and Direct**
   - Use simple, straightforward language
   - Avoid jargon unless necessary (and define it when used)
   - Get to the point quickly

2. **Professional but Approachable**
   - Write in second person ("you") for instructions
   - Use active voice: "Configure the server" not "The server should be configured"
   - Be friendly but not casual

3. **Technically Accurate**
   - Verify all technical information
   - Test all code examples
   - Include version information when relevant

### Examples

✅ **Good**: "To start the server, run `./build/jdbx_runtime.sh start`"

❌ **Avoid**: "You might want to consider possibly starting the server by executing the runtime script"

## File Naming

### Conventions

1. **Use kebab-case**: `authentication-guide.md`
2. **Be descriptive**: `javascript-integration-tutorial.md` not `js.md`
3. **Use standard suffixes**:
   - `-guide.md` for how-to guides
   - `-reference.md` for reference documentation
   - `-tutorial.md` for step-by-step tutorials
   - `-overview.md` for high-level introductions

### Directory Structure

```
api-reference.md          ✓ Correct
API_REFERENCE.md         ❌ Wrong (uppercase)
api.md                   ❌ Wrong (too generic)
ApiReference.md          ❌ Wrong (PascalCase)
```

## Document Structure

### Standard Template

```markdown
# Document Title

> One-line description of what this document covers

## Table of Contents

- [Prerequisites](#prerequisites)
- [Overview](#overview)
- [Main Content](#main-content)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [See Also](#see-also)

## Prerequisites

List what readers need to know or have installed.

## Overview

Brief introduction to the topic (2-3 paragraphs max).

## Main Content

The core information, broken into logical sections.

## Examples

Practical, working examples.

## Troubleshooting

Common issues and solutions.

## See Also

- [Related Document](link)
- [External Resource](link)
```

### Section Headers

- Use Title Case for main title (# Title)
- Use Sentence case for all other headers (## Section name)
- Keep headers concise and descriptive
- Use imperative mood for task-based sections

## Markdown Standards

### Formatting Rules

1. **Headers**
   ```markdown
   # Main Title (only one per document)
   ## Major Section
   ### Subsection
   #### Minor Section (avoid if possible)
   ```

2. **Lists**
   ```markdown
   - Use hyphens for unordered lists
   - Keep items parallel in structure
   
   1. Use numbers for ordered lists
   2. When order matters
   ```

3. **Code Blocks**
   ````markdown
   ```bash
   # Always specify the language
   ./build/jdbx_runtime.sh start
   ```
   
   ```javascript
   // Include helpful comments
   const doc = {
     _id: "user-123",
     name: "John Doe"
   };
   ```
   ````

4. **Inline Code**
   - Use backticks for commands: `npm install`
   - Use backticks for file paths: `/opt/jdbx/config.json`
   - Use backticks for function names: `db_insert_document()`

5. **Tables**
   ```markdown
   | Column 1 | Column 2 | Column 3 |
   |----------|----------|----------|
   | Data     | Data     | Data     |
   ```

### Emphasis

- **Bold** for important concepts or warnings
- *Italic* for emphasis or new terms (first use)
- `code` for technical terms, commands, or values
- > Blockquotes for important notes or tips

## Code Examples

### Best Practices

1. **Complete and Runnable**
   ```javascript
   // ✅ Good: Complete example
   const db = require('jdbx-client');
   
   async function createUser() {
     const client = await db.connect('http://localhost:5000');
     const result = await client.insert('users', {
       name: 'John Doe',
       email: 'john@example.com'
     });
     console.log('Created user:', result._id);
   }
   
   createUser().catch(console.error);
   ```

2. **Include Error Handling**
   ```javascript
   try {
     const result = await client.query('users', { age: { $gt: 18 } });
   } catch (error) {
     console.error('Query failed:', error.message);
   }
   ```

3. **Add Explanatory Comments**
   ```bash
   # Start the server with custom configuration
   JDBX_PORT=8080 ./build/jdbx_runtime.sh start
   
   # Check server status
   ./build/jdbx_runtime.sh status
   ```

### Formatting

- Use 2 spaces for indentation in examples
- Keep lines under 80 characters when possible
- Include output when helpful
- Show both success and error cases

## API Documentation

### Endpoint Documentation

```markdown
### Create Document

Creates a new document in the specified collection.

**Endpoint:** `POST /api/collections/{collection}/documents`

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| collection | string | Yes | Collection name |

**Request Body:**
```json
{
  "name": "John Doe",
  "email": "john@example.com"
}
```

**Response:**
- **200 OK**: Document created successfully
```json
{
  "_id": "doc-1234567890-abcd",
  "name": "John Doe",
  "email": "john@example.com"
}
```

**Errors:**
- **400 Bad Request**: Invalid document format
- **401 Unauthorized**: Missing or invalid authentication
- **409 Conflict**: Document with ID already exists
```

### Function Documentation

```markdown
### db_insert_document()

Inserts a new document into a collection.

**Signature:**
```c
json_value_t* db_insert_document(database_t* db, const char* collection_name, 
                                json_value_t* document);
```

**Parameters:**
- `db`: Database instance
- `collection_name`: Name of the target collection
- `document`: JSON document to insert

**Returns:**
- Success: Pointer to the inserted document with generated `_id`
- Failure: NULL

**Example:**
```c
json_value_t* doc = json_create_object();
json_object_set(doc, "name", json_create_string("John"));

json_value_t* result = db_insert_document(db, "users", doc);
if (result) {
    printf("Inserted with ID: %s\n", 
           json_object_get(result, "_id")->value.string);
    json_free(result);
}
json_free(doc);
```
```

## Cross-References

### Internal Links

- Use relative paths: `[Authentication Guide](../guides/authentication.md)`
- Link to sections: `[Query Syntax](../reference/query-language.md#syntax)`
- Verify all links work

### External Links

- Use descriptive text: `[JSON specification](https://json.org)`
- Not: `[click here](https://json.org)`
- Include link in parentheses for print versions

## Versioning

### Version Information

1. **Document Headers**
   ```markdown
   # Feature Name
   
   > Available since: v2.0.0
   > Last updated: v3.0.0
   ```

2. **Feature Flags**
   ```markdown
   > **Note**: This feature requires JDBX v2.5.0 or later
   ```

3. **Deprecation Notices**
   ```markdown
   > **Deprecated**: This endpoint is deprecated as of v3.0.0. 
   > Use `/api/v2/collections` instead.
   ```

## Maintenance

### Review Checklist

- [ ] All code examples tested and working
- [ ] Links verified (internal and external)
- [ ] Spelling and grammar checked
- [ ] Technical accuracy verified
- [ ] Follows style guide conventions
- [ ] Includes appropriate cross-references
- [ ] Version information is current

### Update Process

1. **Regular Reviews**: Review documentation quarterly
2. **Issue Tracking**: Tag documentation issues in GitHub
3. **Change Log**: Note significant documentation changes
4. **Deprecation**: Mark outdated content clearly

## Common Mistakes to Avoid

1. **Don't duplicate content** - Link to existing documentation
2. **Don't use absolute paths** - Use relative paths for portability
3. **Don't forget examples** - Every concept needs an example
4. **Don't mix concerns** - Keep user and developer docs separate
5. **Don't use unclear pronouns** - Be specific about what "it" refers to

## Quick Reference

### Document Types

| Type | Purpose | Location | Example |
|------|---------|----------|------|
| Guide | How to accomplish tasks | `/guides/` | `authentication-guide.md` |
| Reference | Detailed specifications | `/reference/` | `configuration-reference.md` |
| Tutorial | Step-by-step learning | `/getting-started/` | `quick-start-tutorial.md` |
| API | Endpoint documentation | `/api/` | `rest-api-reference.md` |
| Architecture | System design | `/architecture/` | `system-design-overview.md` |

### Markdown Quick Reference

```markdown
**bold**              *italic*            `code`
[link](url)           ![image](url)       > quote
- unordered list      1. ordered list     --- (horizontal rule)

# H1  ## H2  ### H3   #### H4

| Table | Header |
|-------|--------|
| Cell  | Cell   |
```