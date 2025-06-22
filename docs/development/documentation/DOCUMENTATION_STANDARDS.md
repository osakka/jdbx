# JDBX Documentation Standards

**Version**: 1.0.0  
**Date**: June 22, 2025  
**Status**: Active  

## Documentation Taxonomy

### 1. Directory Structure

```
docs/
├── README.md                    # Navigation hub for all documentation
├── CHANGELOG.md                 # Comprehensive version history
├── architecture/               # System design and architectural decisions
│   ├── README.md              # Architecture overview
│   ├── adr/                   # Architecture Decision Records
│   ├── design/                # Design documents and diagrams
│   └── concepts/              # Core concepts and principles
├── api/                       # API documentation
│   ├── README.md              # API overview
│   ├── rest/                  # REST API reference
│   ├── websocket/             # WebSocket API reference
│   └── examples/              # API usage examples
├── guides/                    # How-to guides and tutorials
│   ├── README.md              # Guide index
│   ├── getting-started/       # Quickstart and installation
│   ├── development/           # Development guides
│   ├── deployment/            # Production deployment
│   └── troubleshooting/       # Problem-solving guides
├── reference/                 # Technical reference
│   ├── README.md              # Reference index
│   ├── configuration/         # Configuration options
│   ├── cli/                   # Command-line interface
│   ├── security/              # Security reference
│   └── performance/           # Performance tuning
├── development/               # Development documentation
│   ├── README.md              # Developer guide
│   ├── contributing/          # Contribution guidelines
│   ├── testing/               # Testing documentation
│   └── release/               # Release process
└── operations/                # Operations documentation
    ├── README.md              # Operations overview
    ├── monitoring/            # Monitoring and metrics
    ├── maintenance/           # Maintenance procedures
    └── troubleshooting/       # Operational troubleshooting
```

### 2. Naming Conventions

#### File Names
- **Use kebab-case**: `architecture-overview.md`, not `Architecture_Overview.md`
- **Be descriptive**: `memory-checkpoint-safety.md`, not `memory.md`
- **Include version for versioned docs**: `api-v2.md`, `migration-v6-to-v7.md`
- **Use standard names**: `README.md`, `CHANGELOG.md`, `LICENSE.md`

#### ADR Naming
- **Format**: `ADR-NNN-descriptive-name.md`
- **Example**: `ADR-040-memory-checkpoint-safety.md`
- **Sequence**: Three-digit zero-padded numbers

#### Document Types
- **Conceptual**: Explains concepts and architecture
- **Tutorial**: Step-by-step learning guides
- **How-to**: Task-oriented guides
- **Reference**: Technical specifications
- **ADR**: Architectural decisions

### 3. Document Structure

#### Front Matter (Required)
```markdown
# Document Title

**Version**: X.Y.Z  
**Date**: YYYY-MM-DD  
**Status**: Draft|Active|Deprecated  
**Category**: Architecture|API|Guide|Reference|Operations  
```

#### Standard Sections
1. **Overview**: Brief description and purpose
2. **Context**: Background and problem statement
3. **Content**: Main documentation body
4. **Examples**: Code examples and usage
5. **References**: Related documents and links

### 4. Content Guidelines

#### Accuracy
- **Verify against code**: All examples must work with current version
- **Version specificity**: Clearly indicate version requirements
- **Update regularly**: Documentation updates with code changes
- **Test commands**: All commands must be tested

#### Clarity
- **Active voice**: "The server processes requests" not "Requests are processed"
- **Present tense**: "The API returns" not "The API will return"
- **Concise**: One idea per paragraph
- **Examples**: Include examples for complex concepts

#### Consistency
- **Terminology**: Use consistent terms throughout
- **Formatting**: Follow markdown standards
- **Code style**: Match project code style
- **Cross-references**: Use relative links

### 5. Code Examples

#### Format
```markdown
```language
// Comment explaining the example
code here
```
```

#### Requirements
- **Executable**: Examples must run without modification
- **Complete**: Include all imports and setup
- **Annotated**: Comments explain key points
- **Tested**: Verify examples work

### 6. Versioning

#### Document Versions
- **Major**: Significant content changes
- **Minor**: Additions and clarifications
- **Patch**: Typo and formatting fixes

#### API Versions
- **Document all versions**: Keep historical API docs
- **Mark deprecations**: Clear deprecation notices
- **Migration guides**: Version-to-version guides

### 7. Review Process

1. **Technical Review**: Verify accuracy against implementation
2. **Editorial Review**: Grammar, clarity, consistency
3. **Code Review**: Test all examples
4. **Cross-reference**: Verify all links work

### 8. Maintenance

#### Regular Tasks
- **Quarterly audit**: Full documentation review
- **Release updates**: Update with each release
- **Link validation**: Check all links monthly
- **Example testing**: Automated testing of examples

#### Deprecation
- **Mark clearly**: Add deprecation notice
- **Provide alternative**: Link to replacement
- **Set removal date**: Plan deprecation timeline
- **Keep historical**: Archive, don't delete

## Implementation Checklist

- [ ] Reorganize existing docs into taxonomy
- [ ] Rename files to match conventions
- [ ] Add front matter to all documents
- [ ] Verify all code examples
- [ ] Create missing category READMEs
- [ ] Update cross-references
- [ ] Eliminate duplicates
- [ ] Create comprehensive index

## Tools

### Documentation Linting
```bash
# Check markdown formatting
markdownlint docs/

# Verify links
markdown-link-check docs/**/*.md

# Test code examples
./scripts/test-doc-examples.sh
```

### Generation
```bash
# Generate API docs from code
./scripts/generate-api-docs.sh

# Create changelog from git
./scripts/generate-changelog.sh
```

## References

- [Diátaxis Framework](https://diataxis.fr/)
- [Google Developer Documentation Style Guide](https://developers.google.com/style)
- [Microsoft Writing Style Guide](https://docs.microsoft.com/style-guide)
- [Semantic Versioning](https://semver.org/)