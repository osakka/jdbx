# JDBX Architecture Decision Records (ADR)

**Last Updated**: June 18, 2025  
**Current Version**: v6.5.6

## Overview

This directory contains Architecture Decision Records (ADRs) - documents that capture important architectural decisions made during JDBX development. Each ADR describes a single decision, its context, and consequences.

## ADR Index

### Foundation Decisions (May 2025)
- [ADR-001: JavaScript Integration with QuickJS](001-javascript-integration.md)
- [ADR-002: Component-Based Architecture](002-component-architecture.md)
- [ADR-003: Thread Pool Architecture](003-thread-pool.md)
- [ADR-004: Socket Binding Improvements](004-socket-binding.md)

### Binary Persistence (v2.0.0)
- [ADR-005: Binary Persistence System](005-binary-persistence.md)
- [ADR-006: Environment-Based Configuration](006-environment-config.md)
- [ADR-007: Database-Based RBAC](007-database-rbac.md)
- [ADR-008: Unified Buffer Pool](008-buffer-pool.md)

### High-Performance Architecture (v3.0.0)
- [ADR-009: Billion-Document Scale Architecture](009-billion-document-scale.md)
- [ADR-010: JDBX Storage Backend](010-storage-backend.md)
- [ADR-011: Adaptive Indexing System](011-adaptive-indexing.md)
- [ADR-012: SSL/TLS by Default](012-ssl-default.md)

### Unified Documents Architecture (v4.0.0-v6.0.0)
- [ADR-013: Everything is a Document](013-everything-document.md)
- [ADR-014: Lock-Free Architecture](014-lock-free.md)
- [ADR-015: JDBX Rebranding](015-rebranding.md)
- [ADR-016: UUID-Only Identification](016-uuid-only.md)
- [ADR-017: TRUE Unified Architecture](017-true-unified-architecture.md)

### Enterprise Security (v6.1.0-v6.2.0)
- [ADR-018: Memory Corruption Resolution](018-memory-corruption.md)
- [ADR-019: Three-Tier Configuration](019-three-tier-config.md)
- [ADR-020: Zero Hardcoded Values](020-zero-hardcoded.md)
- [ADR-021: Enterprise CLI](021-enterprise-cli.md)

### Revolutionary Memory Management (v6.3.0-v6.5.6)
- [ADR-022: Checkpoint-Based Memory Management](022-checkpoint-memory-management.md)
- [ADR-023: JSON Memory Management Checkpoint Integration](023-json-checkpoint-integration.md) - **NEW: v6.5.6**

### Supporting Decisions
- [ADR-028: Atomic Naming Standards](028-atomic-naming.md)
- [ADR-029: Zero-Warning Compilation](029-zero-warnings.md)
- [ADR-030: Single Source of Truth](030-single-source-truth.md)

## Complete Timeline

For a comprehensive chronological view of all decisions, see:
- [Complete ADR Timeline](COMPLETE_ADR.md) - All decisions with full context
- [Architectural Timeline](timeline.md) - Version-by-version evolution

## ADR Template

When creating new ADRs, use this template:

```markdown
# ADR-NNN: Title

**Date**: YYYY-MM-DD  
**Status**: Proposed/Active/Superseded  
**Deciders**: List of people involved  
**Technical Story**: Brief description

## Context and Problem Statement

What is the issue that we're seeing that is motivating this decision?

## Decision Drivers

- Driver 1
- Driver 2

## Considered Options

1. Option 1
2. Option 2
3. Option 3

## Decision Outcome

Chosen option: "Option N" because...

### Positive Consequences

- Good thing 1
- Good thing 2

### Negative Consequences

- Bad thing 1
- Bad thing 2

## Implementation Details

Technical details, code examples, etc.

## Links

- Related documentation
- Issue trackers
```

## Decision Status

- **Proposed**: Under discussion
- **Active**: Implemented and in use
- **Superseded**: Replaced by another ADR
- **Deprecated**: No longer relevant

## Principles

1. **Immutability**: ADRs are never deleted, only superseded
2. **Brevity**: Keep ADRs focused on a single decision
3. **Context**: Always explain why, not just what
4. **Consequences**: Document both positive and negative impacts
5. **Traceability**: Link to commits, issues, and related documentation