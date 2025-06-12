# JSONdb Documentation Taxonomy & Standards

**Version**: 3.3.0  
**Last Updated**: June 12, 2025

## Documentation Taxonomy

This document defines the authoritative taxonomy and naming standards for all JSONdb documentation.

### Primary Categories

#### 1. **Root Level** (`/`)
- `README.md` - Primary project overview and navigation hub
- `CHANGELOG.md` - Complete version history and release notes  
- `CLAUDE.md` - Development guidelines and project conventions

#### 2. **Core Documentation** (`/docs/`)
- `README.md` - Documentation index and navigation
- **No other files** - All content must be categorized

#### 3. **Getting Started** (`/docs/getting-started/`)
- `installation.md` - System requirements and installation
- `quick-start.md` - 5-minute setup guide
- `configuration.md` - Basic configuration settings
- `first-application.md` - Build your first app tutorial

#### 4. **Guides** (`/docs/guides/`)
- `authentication.md` - Authentication and security setup
- `javascript-development.md` - JavaScript integration guide  
- `production-deployment.md` - Production deployment guide
- `performance-tuning.md` - Performance optimization guide
- `rbac-setup.md` - Role-based access control setup

#### 5. **API Reference** (`/docs/api/`)
- `rest-api.md` - Complete REST API documentation
- `javascript-api.md` - JavaScript integration API
- `client-libraries.md` - Language-specific SDKs
- `authentication-api.md` - Authentication endpoints
- `rbac-api.md` - RBAC management API

#### 6. **Architecture** (`/docs/architecture/`)
- `overview.md` - High-level architecture overview
- `jdbx-storage.md` - JDBX storage backend design
- `lock-free-operations.md` - Lock-free architecture design
- `unified-documents.md` - Unified documents architecture
- `field-level-operations.md` - Field-level RBAC and operations

#### 7. **Reference** (`/docs/reference/`)
- `configuration.md` - Complete configuration reference
- `logging.md` - Logging format and standards
- `metrics.md` - Metrics and monitoring reference
- `performance-benchmarks.md` - Performance data and benchmarks
- `error-codes.md` - Error codes and troubleshooting

#### 8. **Development** (`/docs/development/`)
- `building.md` - Build system and compilation
- `testing.md` - Testing framework and procedures
- `contributing.md` - Contribution guidelines
- `style-guide.md` - Code and documentation style guide

#### 9. **Examples** (`/docs/examples/`)
- `basic-operations.md` - Basic CRUD operations
- `advanced-queries.md` - Complex query examples
- `javascript-functions.md` - JavaScript function examples
- `production-configs.md` - Production configuration examples

### Naming Standards

#### File Naming Convention
- **Format**: `kebab-case.md`
- **Examples**: `quick-start.md`, `rest-api.md`, `lock-free-operations.md`
- **No underscores or CamelCase in filenames**

#### Content Structure Standards
1. **YAML Front Matter** (when applicable):
   ```yaml
   ---
   title: "Document Title"
   version: "3.3.0"
   last_updated: "2025-12-06"
   category: "guides"
   tags: ["authentication", "security"]
   ---
   ```

2. **Header Structure**:
   - H1: Document title
   - H2: Major sections  
   - H3: Subsections
   - H4: Detail sections (max depth)

3. **Cross-Reference Format**:
   - Internal: `[Text](../category/document.md)`
   - External: `[Text](https://example.com)`
   - Code: `src/components/path/file.c:123`

### Content Quality Standards

#### Technical Accuracy Requirements
- All code examples must compile and execute correctly
- API documentation must match actual v3.3.0 implementation
- Configuration examples must use current parameter names
- Performance data must reflect current benchmarks

#### Documentation Completeness
- Every public API endpoint documented
- Every configuration parameter explained
- Every error code documented with solutions
- Every major feature has usage guide

#### Maintenance Standards
- Version numbers updated with each release
- Deprecated features marked clearly
- Migration guides for breaking changes
- Regular accuracy audits against codebase

### Exclusions and Archives

#### Files NOT in Main Documentation
- Implementation notes (internal development)
- Historical design decisions (unless architecturally relevant)
- Debug logs and investigation reports
- Temporary analysis documents
- Draft proposals and planning documents

#### Archive Policy
- Archive location: `docs-archive/`
- Archive naming: `YYYY-MM-DD/original-structure/`
- Archive retention: 2 years for historical reference
- No active maintenance of archived content

### Cross-Reference Index

All documentation must maintain bi-directional links:
- Parent documents link to children
- Child documents link to parents
- Related documents cross-reference each other
- API docs link to implementation guides
- Guides link to reference material

This taxonomy ensures maintainable, accurate, and navigable documentation that serves both new users and expert developers effectively.