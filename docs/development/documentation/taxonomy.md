# JDBX Documentation Taxonomy and Standards

## Directory Structure

```
docs/
├── README.md                    # Documentation portal and navigation guide
├── architecture/               # System design and technical architecture
│   ├── README.md              # Architecture overview
│   ├── adr/                   # Architecture Decision Records
│   ├── core-concepts/         # Fundamental concepts and patterns
│   ├── performance/           # Performance architecture and optimization
│   └── security/              # Security architecture and design
├── getting-started/           # Quick start and initial setup
│   ├── README.md             # Getting started overview
│   ├── installation.md       # Installation instructions
│   ├── quick-start.md        # 5-minute quick start
│   └── first-app.md          # Building your first JDBX app
├── tutorials/                 # Step-by-step learning paths
│   ├── README.md             # Tutorial index
│   ├── beginner/             # Basic concepts and operations
│   ├── intermediate/         # Advanced features and patterns
│   └── advanced/             # Expert-level techniques
├── how-to/                    # Task-oriented guides
│   ├── README.md             # How-to index
│   ├── operations/           # Operational tasks
│   ├── development/          # Development tasks
│   └── troubleshooting/      # Problem-solving guides
├── reference/                 # Technical specifications
│   ├── README.md             # Reference overview
│   ├── api/                  # API documentation
│   ├── configuration/        # Configuration reference
│   ├── query-language/       # Query syntax reference
│   └── specifications/       # Technical specifications
├── deployment/                # Production deployment
│   ├── README.md             # Deployment overview
│   ├── production/           # Production setup
│   ├── platforms/            # Platform-specific guides
│   └── operations/           # Operational procedures
├── development/               # For contributors
│   ├── README.md             # Development overview
│   ├── contributing/         # Contribution guidelines
│   ├── building/             # Build instructions
│   ├── testing/              # Testing procedures
│   ├── documentation/        # Documentation standards
│   └── changelog.md          # Version history
├── security/                  # Security documentation
│   ├── README.md             # Security overview
│   ├── guidelines/           # Security best practices
│   ├── compliance/           # Compliance guides
│   └── threat-modeling/      # Security assessments
└── examples/                  # Code examples
    ├── README.md             # Examples overview
    ├── basic/                # Basic usage examples
    ├── advanced/             # Advanced patterns
    └── templates/            # Project templates
```

## Naming Standards

### File Naming Convention
- **Format**: `kebab-case.md`
- **Language**: English, descriptive, action-oriented
- **Examples**:
  - ✅ `installation-guide.md`
  - ✅ `query-optimization.md`
  - ✅ `ssl-configuration.md`
  - ❌ `install.md` (too brief)
  - ❌ `SSL_Config.md` (wrong case)
  - ❌ `guide-to-installation.md` (redundant)

### Document Types and Prefixes
- **Guides**: Action-oriented (`configuring-ssl.md`)
- **References**: Noun-based (`api-reference.md`)
- **Concepts**: Descriptive (`unified-documents.md`)
- **Tutorials**: Progressive (`01-basic-queries.md`)

## Content Standards

### Document Structure
1. **Title** (H1): Clear, descriptive
2. **Overview**: Brief introduction (2-3 sentences)
3. **Prerequisites**: Required knowledge/setup
4. **Content**: Logical progression
5. **Summary**: Key takeaways
6. **Related**: Links to related docs

### Writing Style
- **Voice**: Active, second person ("you")
- **Tense**: Present tense
- **Clarity**: Short sentences, clear language
- **Examples**: Code examples for every concept
- **Accuracy**: Verified against codebase

## Documentation Categories

### 1. Architecture (`/architecture`)
**Purpose**: Technical design and decisions
**Audience**: Architects, senior developers
**Content**: ADRs, system design, patterns

### 2. Getting Started (`/getting-started`)
**Purpose**: Onboarding new users
**Audience**: First-time users
**Content**: Installation, quick start, basics

### 3. Tutorials (`/tutorials`)
**Purpose**: Learning paths
**Audience**: Developers learning JDBX
**Content**: Progressive, hands-on guides

### 4. How-To Guides (`/how-to`)
**Purpose**: Specific task completion
**Audience**: Users with specific goals
**Content**: Step-by-step instructions

### 5. Reference (`/reference`)
**Purpose**: Technical specifications
**Audience**: Developers needing details
**Content**: APIs, configs, specifications

### 6. Deployment (`/deployment`)
**Purpose**: Production deployment
**Audience**: DevOps, administrators
**Content**: Setup, configuration, operations

### 7. Development (`/development`)
**Purpose**: Contributing to JDBX
**Audience**: Contributors, maintainers
**Content**: Build, test, contribute

### 8. Security (`/security`)
**Purpose**: Security documentation
**Audience**: Security teams, auditors
**Content**: Best practices, compliance

### 9. Examples (`/examples`)
**Purpose**: Working code samples
**Audience**: Developers
**Content**: Code examples, templates

## Cross-Reference System

### Internal Links
- Use relative paths: `[API Reference](../reference/api/)`
- Link to sections: `[Authentication](#authentication)`
- Verify all links work

### External Links
- Mark clearly: `[External: Docker Docs]`
- Use HTTPS always
- Check link validity

## Version Management

### Documentation Versioning
- Match software version
- Tag documentation releases
- Maintain version history

### Change Tracking
- Update changelog.md
- Note breaking changes
- Document deprecations

## Quality Checklist

- [ ] Accurate against codebase
- [ ] No duplicate content
- [ ] Proper categorization
- [ ] Working cross-references
- [ ] Consistent formatting
- [ ] Clear navigation
- [ ] Updated changelog
- [ ] Version consistency