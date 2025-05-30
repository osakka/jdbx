# JSONdb Documentation Standards

## Document Naming Schema

### 1. File Naming Conventions

All documentation files follow these naming conventions:

```
<category>-<topic>[-<subtopic>].md
```

**Categories:**
- `api` - API endpoint documentation
- `guide` - User guides and tutorials
- `reference` - Technical reference documentation
- `architecture` - System design and architecture
- `operations` - Deployment and operations guides
- `development` - Developer documentation
- `changelog` - Version history

**Examples:**
- `api-rest.md` - REST API reference
- `guide-quickstart.md` - Getting started guide
- `reference-configuration.md` - Configuration reference
- `architecture-components.md` - Component architecture

### 2. Document Types

#### API Documentation
- **Location**: `/docs/api/`
- **Naming**: `api-<endpoint-group>.md`
- **Content**: Endpoint definitions, request/response formats, examples

#### User Guides
- **Location**: `/docs/guides/`
- **Naming**: `guide-<topic>.md`
- **Content**: Step-by-step instructions, tutorials, best practices

#### Technical Reference
- **Location**: `/docs/reference/`
- **Naming**: `reference-<topic>.md`
- **Content**: Detailed technical information, specifications

#### Architecture Documentation
- **Location**: `/docs/architecture/`
- **Naming**: `architecture-<component>.md`
- **Content**: System design, component interactions, diagrams

### 3. Document Structure

Every document MUST include:

```markdown
# [Document Title]

**Status**: [Draft|Review|Stable|Deprecated]  
**Version**: [Document version]  
**Last Updated**: [Date]  
**Category**: [Category name]

## Overview
Brief description of what this document covers.

## Table of Contents
- [Section 1](#section-1)
- [Section 2](#section-2)

## Content
[Main content]

## See Also
- [Related Document 1](link)
- [Related Document 2](link)
```

### 4. Content Standards

#### Writing Style
- Use clear, concise language
- Write in present tense
- Use active voice
- Define acronyms on first use
- Include examples for complex concepts

#### Code Examples
- Use syntax highlighting
- Include complete, runnable examples
- Show expected output
- Explain any prerequisites

#### Versioning
- Document the JSONdb version the documentation applies to
- Note any version-specific features
- Mark deprecated features clearly

### 5. Cross-References

#### Internal Links
Use relative paths from document location:
```markdown
See [Configuration Guide](../reference/reference-configuration.md)
```

#### External Links
Always use HTTPS when available:
```markdown
See [JSON Specification](https://www.json.org/)
```

### 6. Special Sections

#### Warnings and Notes
```markdown
> **Warning**: Critical information

> **Note**: Additional information

> **Tip**: Helpful suggestion
```

#### API Endpoints
```markdown
### `GET /api/collections`

**Description**: List all collections

**Authentication**: Required

**Request**:
```http
GET /api/collections
Authorization: Bearer <token>
```

**Response**:
```json
{
  "collections": [...]
}
```
```

### 7. Documentation Hierarchy

```
/docs/
├── README.md                    # Documentation index
├── DOCUMENTATION_STANDARDS.md   # This file
├── api/                        # API documentation
│   ├── api-rest.md            # REST API reference
│   ├── api-javascript.md      # JavaScript API
│   └── api-rbac.md            # RBAC API
├── guides/                     # User guides
│   ├── guide-quickstart.md    # Getting started
│   ├── guide-javascript.md    # Using JavaScript
│   └── guide-production.md    # Production deployment
├── reference/                  # Technical reference
│   ├── reference-configuration.md
│   ├── reference-query-language.md
│   └── reference-metrics.md
├── architecture/              # Architecture docs
│   ├── architecture-overview.md
│   ├── architecture-components.md
│   └── architecture-binary-format.md
└── operations/               # Operations guides
    ├── operations-deployment.md
    ├── operations-monitoring.md
    └── operations-troubleshooting.md
```

### 8. Consolidation Rules

When consolidating duplicate documentation:

1. **Identify the most recent and accurate version**
2. **Merge unique content from other versions**
3. **Archive old versions with clear deprecation notice**
4. **Update all references to point to consolidated document**
5. **Add redirect notices in old locations**

### 9. Quality Checklist

Before publishing documentation:

- [ ] Follows naming conventions
- [ ] Includes all required sections
- [ ] Code examples are tested and working
- [ ] Links are valid and use correct paths
- [ ] Grammar and spelling are correct
- [ ] Technical accuracy verified
- [ ] Version information is current
- [ ] Table of contents matches content

### 10. Maintenance

#### Regular Reviews
- Quarterly accuracy review
- Update examples for new versions
- Remove deprecated content
- Fix broken links

#### Change Process
1. Update documentation with code changes
2. Review for technical accuracy
3. Update version and date
4. Update index files
5. Commit with descriptive message

## Implementation Plan

### Phase 1: Consolidation (Immediate)
1. Remove duplicate API documentation
2. Consolidate RBAC documentation
3. Merge socket binding documentation
4. Combine configuration documentation

### Phase 2: Reorganization (Week 1)
1. Create new directory structure
2. Rename files to follow standards
3. Update all cross-references
4. Create redirect notices

### Phase 3: Content Update (Week 2)
1. Update all documents to current version
2. Add missing documentation
3. Verify all code examples
4. Complete accuracy audit

### Phase 4: Index Creation (Week 3)
1. Create comprehensive index
2. Add navigation aids
3. Generate search metadata
4. Publish documentation map