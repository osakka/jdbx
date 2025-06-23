# Architecture Decision Records (ADRs)

This directory contains all Architecture Decision Records for the JDBX project.

## What is an ADR?

An Architecture Decision Record (ADR) is a document that captures an important architectural decision made along with its context and consequences.

## ADR Format

Each ADR follows this structure:
- **Title**: ADR-NNN-descriptive-name.md (where NNN is a sequential number)
- **Status**: Proposed, Accepted, Deprecated, or Superseded
- **Context**: What is the issue that we're seeing that is motivating this decision?
- **Decision**: What is the change that we're proposing and/or doing?
- **Consequences**: What becomes easier or more difficult to do because of this change?

## Current ADRs

The ADRs are numbered sequentially from 001 to 046 (as of June 2025). Key architectural decisions include:

- JavaScript integration (ADR-001)
- Lock-free architecture (ADR-003)  
- Unified documents architecture (ADR-027)
- Checkpoint-based memory manager (ADR-028)
- Revolutionary ART engine implementation (ADR-046)

## Creating New ADRs

When creating a new ADR:
1. Use the next sequential number
2. Follow the naming convention: `ADR-NNN-descriptive-name.md`
3. Use the standard ADR template
4. Link to related ADRs when applicable

## Note

For general architecture documentation (overviews, diagrams, reports), see the `docs/architecture/` directory.