# JDBX Documentation Taxonomy

**Professional Documentation Organization Schema v1.0**

## Industry Standards Compliance

This taxonomy follows the **Diátaxis Framework** - the industry-standard documentation architecture used by Django, Kubernetes, and other enterprise software projects.

### Four Documentation Types (Diátaxis Framework)

| Type | Purpose | User Goal | Content Format |
|------|---------|-----------|----------------|
| **Tutorials** | Learning-oriented | "I want to learn" | Step-by-step lessons |
| **How-To Guides** | Problem-oriented | "I want to solve X" | Goal-oriented recipes |
| **Reference** | Information-oriented | "I want to look up X" | Technical specifications |
| **Explanation** | Understanding-oriented | "I want to understand" | Concepts and context |

## JDBX Documentation Categories

### 1. **Getting Started** (`/getting-started/`)
**Type**: Tutorial (Learning-oriented)
**Purpose**: First-time user onboarding and initial setup

**Files**:
- `README.md` - Category overview and quick navigation
- `installation.md` - System requirements and installation
- `quick-start.md` - 5-minute setup and first operations
- `basic-operations.md` - Essential CRUD operations

### 2. **Tutorials** (`/tutorials/`)
**Type**: Tutorial (Learning-oriented)
**Purpose**: Progressive skill-building courses

**Structure**:
```
tutorials/
├── README.md
├── beginner/
│   ├── README.md
│   ├── data-modeling.md
│   ├── user-authentication.md
│   └── basic-queries.md
├── intermediate/
│   ├── README.md
│   ├── rbac-setup.md
│   ├── javascript-functions.md
│   └── performance-tuning.md
└── advanced/
    ├── README.md
    ├── custom-indexing.md
    ├── clustering.md
    └── enterprise-integration.md
```

### 3. **How-To Guides** (`/how-to/`)
**Type**: How-To (Problem-oriented)
**Purpose**: Solution-focused guides for specific tasks

**Structure**:
```
how-to/
├── README.md
├── operations/
│   ├── README.md
│   ├── backup-restore.md
│   ├── monitoring-setup.md
│   └── ssl-configuration.md
├── development/
│   ├── README.md
│   ├── custom-validators.md
│   ├── debugging.md
│   └── migrations.md
└── troubleshooting/
    ├── README.md
    ├── common-issues.md
    └── performance-problems.md
```

### 4. **Reference** (`/reference/`)
**Type**: Reference (Information-oriented)
**Purpose**: Technical specifications and API documentation

**Structure**:
```
reference/
├── README.md
├── api/
│   ├── README.md
│   ├── rest-api.md
│   ├── javascript.md
│   ├── authentication.md
│   └── error-codes.md
├── configuration/
│   ├── README.md
│   ├── server-config.md
│   └── database-config.md
├── query-language/
│   ├── README.md
│   ├── syntax.md
│   └── operators.md
└── specifications/
    ├── README.md
    ├── performance-specs.md
    └── compatibility.md
```

### 5. **Architecture** (`/architecture/`)
**Type**: Explanation (Understanding-oriented)
**Purpose**: System design and technical deep-dives

**Structure**:
```
architecture/
├── README.md
├── core-concepts/
│   ├── README.md
│   ├── unified-documents.md
│   ├── memory-management.md
│   └── threading-model.md
├── security/
│   ├── README.md
│   ├── rbac-design.md
│   └── authentication.md
├── performance/
│   ├── README.md
│   ├── optimization-guide.md
│   └── scaling-strategies.md
└── design-decisions/
    ├── README.md
    └── architectural-decisions/
        ├── README.md
        ├── 001-javascript-integration.md
        └── timeline.md
```

### 6. **Deployment** (`/deployment/`)
**Type**: How-To (Problem-oriented)
**Purpose**: Production deployment and operations

**Structure**:
```
deployment/
├── README.md
├── production/
│   ├── README.md
│   ├── production-checklist.md
│   └── security-hardening.md
├── platforms/
│   ├── README.md
│   ├── docker.md
│   ├── kubernetes.md
│   └── cloud-platforms.md
├── scaling/
│   ├── README.md
│   ├── horizontal-scaling.md
│   └── vertical-scaling.md
└── operations/
    ├── README.md
    ├── backup-strategy.md
    ├── monitoring.md
    └── disaster-recovery.md
```

### 7. **Security** (`/security/`)
**Type**: Reference + How-To
**Purpose**: Security guidelines and compliance

**Structure**:
```
security/
├── README.md
├── guidelines/
│   ├── README.md
│   ├── security-best-practices.md
│   └── access-control.md
├── compliance/
│   ├── README.md
│   ├── gdpr.md
│   └── sox.md
└── threat-modeling/
    ├── README.md
    └── security-assessments.md
```

### 8. **Development** (`/development/`)
**Type**: How-To + Reference
**Purpose**: For contributors and maintainers

**Structure**:
```
development/
├── README.md
├── contributing/
│   ├── README.md
│   ├── contributing.md
│   └── git-workflow.md
├── building/
│   ├── README.md
│   ├── build-instructions.md
│   └── testing-procedures.md
├── documentation/
│   ├── README.md
│   ├── writing-guide.md
│   ├── style-guide.md
│   └── taxonomy.md
└── processes/
    ├── README.md
    ├── release-management.md
    └── todo-tracking.md
```

### 9. **Examples** (`/examples/`)
**Type**: Tutorial + Reference
**Purpose**: Working code examples and templates

**Structure**:
```
examples/
├── README.md
├── basic/
│   ├── README.md
│   ├── crud-operations.json
│   └── simple-queries.js
├── advanced/
│   ├── README.md
│   ├── function-embedding.json
│   └── complex-patterns.js
├── templates/
│   ├── README.md
│   ├── configuration/
│   └── deployment/
└── integrations/
    ├── README.md
    ├── client-libraries/
    └── rest-api-usage/
```

## Naming Conventions

### File Naming Standards
- **Kebab-case**: All files use lowercase with hyphens (`performance-tuning.md`)
- **Descriptive Names**: Clear, unambiguous file names
- **No Abbreviations**: Full words unless industry standard (API, REST, RBAC)

### Directory Naming Standards
- **Lowercase**: All directories use lowercase
- **Plural Nouns**: Categories use plural form (`tutorials/`, `examples/`)
- **Descriptive**: Clear purpose indication

### Document Titles
- **Title Case**: All document titles use proper title case
- **Action-Oriented**: How-to guides start with verbs ("Configure SSL", "Debug Performance")
- **Noun-Based**: Reference docs use noun phrases ("API Reference", "Query Language")

## Navigation Standards

### README Files
- Every directory MUST have a `README.md` file
- README serves as category index and navigation hub
- Include overview, quick links, and sub-category descriptions

### Cross-References
- Use relative links within documentation
- Include "See also" sections for related content
- Maintain bidirectional links where appropriate

### Version Compliance
- All documents must reflect current codebase version (v6.5.0)
- Include "Last Updated" dates in major documents
- Maintain version history in changelog references

## Quality Standards

### Content Requirements
- **Accuracy**: All technical details verified against codebase
- **Completeness**: No placeholder content in production docs
- **Professional Tone**: Consistent voice and terminology
- **Code Examples**: All examples must be tested and functional

### Maintenance Process
- Monthly accuracy audits against codebase changes
- Quarterly taxonomy review and optimization
- Immediate updates for breaking changes
- Version tags for all major documentation releases

---

**Taxonomy Version**: 1.0  
**Created**: June 17, 2025  
**Compliance**: Diátaxis Framework, Industry Best Practices