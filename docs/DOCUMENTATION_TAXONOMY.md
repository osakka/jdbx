# JDBX Documentation Taxonomy

**Version**: 6.5.12  
**Last Updated**: June 18, 2025  
**Purpose**: Establish consistent naming and organization standards for all JDBX documentation

## 1. Documentation Framework

JDBX follows the **Diátaxis Framework** for technical documentation organization:

### Core Categories

1. **Tutorials** (`/tutorials/`)
   - Learning-oriented
   - Take readers through a series of steps
   - Focus: Learning by doing

2. **How-To Guides** (`/how-to/`)
   - Goal-oriented
   - Show how to solve specific problems
   - Focus: Practical results

3. **Reference** (`/reference/`)
   - Information-oriented
   - Describe technical details
   - Focus: Accurate and complete information

4. **Explanation** (`/architecture/`)
   - Understanding-oriented
   - Explain concepts and design decisions
   - Focus: Context and rationale

## 2. File Naming Standards

### General Rules

1. **Use kebab-case**: `my-document-name.md` (NOT: MyDocumentName.md, my_document_name.md)
2. **Be descriptive**: `authentication-setup.md` (NOT: auth.md)
3. **Use `.md` extension**: All documentation in Markdown format
4. **No version numbers in filenames**: Use git for versioning
5. **English only**: All documentation in English

### Category-Specific Naming

#### Tutorials
- Format: `{level}-{topic}.md`
- Examples: `beginner-quick-start.md`, `advanced-clustering.md`

#### How-To Guides
- Format: `{action}-{target}.md`
- Examples: `configure-ssl.md`, `backup-database.md`

#### Reference
- Format: `{component}-reference.md` or `{component}.md`
- Examples: `api-reference.md`, `configuration.md`

#### Architecture (Explanation)
- ADRs: `{number}-{decision-name}.md`
- Examples: `001-javascript-integration.md`
- Core concepts: `{concept}.md`
- Examples: `unified-documents.md`

## 3. Directory Structure

```
docs/
├── README.md                    # Documentation hub and navigation
├── DOCUMENTATION_TAXONOMY.md    # This file
│
├── getting-started/            # Beginner entry points
│   ├── README.md              # Getting started overview
│   ├── installation.md        # Installation instructions
│   └── quick-start.md         # 5-minute guide
│
├── tutorials/                  # Learning paths
│   ├── README.md              # Tutorial index
│   ├── beginner/              # Entry-level tutorials
│   ├── intermediate/          # Building on basics
│   └── advanced/              # Complex scenarios
│
├── how-to/                    # Problem-solving guides
│   ├── README.md              # How-to index
│   ├── operations/            # Operational tasks
│   ├── development/           # Development tasks
│   └── troubleshooting/       # Problem resolution
│
├── reference/                 # Technical specifications
│   ├── README.md              # Reference index
│   ├── api/                   # API documentation
│   ├── configuration/         # Config options
│   ├── query-language/        # Query syntax
│   └── specifications/        # Technical specs
│
├── architecture/              # System design and decisions
│   ├── README.md              # Architecture overview
│   ├── adr/                   # Architecture Decision Records
│   ├── core-concepts/         # Fundamental concepts
│   ├── security/              # Security architecture
│   └── performance/           # Performance design
│
├── security/                  # Security documentation
│   ├── README.md              # Security overview
│   ├── guidelines/            # Security best practices
│   ├── compliance/            # Compliance guides
│   └── threat-modeling/       # Security assessments
│
├── deployment/                # Production deployment
│   ├── README.md              # Deployment overview
│   ├── production/            # Production setup
│   ├── platforms/             # Platform-specific guides
│   ├── scaling/               # Scaling strategies
│   └── operations/            # Operational procedures
│
├── development/               # For contributors
│   ├── README.md              # Development overview
│   ├── contributing/          # Contribution guidelines
│   ├── building/              # Build instructions
│   ├── documentation/         # Doc writing guide
│   └── changelog.md           # Version history
│
└── examples/                  # Working examples
    ├── README.md              # Examples overview
    ├── basic/                 # Simple examples
    ├── advanced/              # Complex examples
    ├── templates/             # Configuration templates
    └── integrations/          # Integration examples
```

## 4. Content Standards

### Document Structure

Every document should include:

1. **Title** (H1): Clear, descriptive title
2. **Metadata** (if applicable):
   - Version
   - Last Updated
   - Prerequisites
3. **Introduction**: Brief overview of the content
4. **Main Content**: Organized with clear headings
5. **Next Steps** (if applicable): Links to related content

### Formatting Guidelines

1. **Headings**: Use hierarchical structure (H1 → H2 → H3)
2. **Code Blocks**: Always specify language for syntax highlighting
3. **Links**: Use relative links for internal documentation
4. **Lists**: Use numbered lists for sequences, bullets for unordered items
5. **Tables**: Use for structured data comparison
6. **Emphasis**: Use **bold** for important points, *italic* for terms

### Version References

- Always use actual version numbers (e.g., "v6.5.12")
- Avoid "latest" or "current" without specific version
- Update version references when documentation is updated

## 5. Special Documentation Types

### Architecture Decision Records (ADRs)

Format: `{number}-{decision-name}.md`

Required sections:
- Title
- Status (Proposed, Accepted, Deprecated, Superseded)
- Context
- Decision
- Consequences

### API Documentation

Required elements:
- Endpoint URL
- HTTP method
- Request/response format
- Authentication requirements
- Example requests/responses
- Error codes

### Configuration Documentation

Required elements:
- Parameter name
- Type
- Default value
- Description
- Example usage
- Related parameters

## 6. Maintenance Guidelines

### Regular Reviews

1. **Version Updates**: Update all version references with new releases
2. **Accuracy Checks**: Verify documentation matches implementation
3. **Link Validation**: Check all internal and external links
4. **Consistency**: Ensure naming and formatting consistency

### Deprecation Process

1. Mark deprecated content clearly
2. Provide migration path
3. Set removal date
4. Update related documentation

### Quality Checklist

Before committing documentation:
- [ ] Follows naming conventions
- [ ] Placed in correct category
- [ ] Version numbers accurate
- [ ] Links work correctly
- [ ] Code examples tested
- [ ] Formatting consistent
- [ ] No duplicate content

## 7. Documentation Principles

1. **Clarity**: Write for your audience's level
2. **Accuracy**: Verify all technical details
3. **Completeness**: Cover all necessary information
4. **Consistency**: Follow established patterns
5. **Maintainability**: Make updates easy
6. **Discoverability**: Use clear organization and naming

## 8. Prohibited Practices

1. **No duplicate documentation**: Single source of truth
2. **No version numbers in filenames**: Use git
3. **No outdated examples**: Keep code current
4. **No broken links**: Verify before committing
5. **No inconsistent terminology**: Use glossary terms
6. **No mixed languages**: English only

This taxonomy ensures JDBX documentation remains professional, discoverable, and maintainable as the project grows.