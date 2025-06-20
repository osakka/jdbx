# ADR Maintenance Guide

**Last Updated**: June 20, 2025  
**Version**: 1.0  
**Purpose**: Ensure consistent and maintainable architectural decision documentation

## Overview

This guide ensures that architectural decisions are properly documented and the ADR system remains maintainable as JDBX evolves.

## ADR Numbering Scheme

### Current Ranges
- **001-026**: Reserved for early architectural decisions (to be backfilled)
- **027-038**: Current documented decisions (June 2025)
- **039+**: Future decisions

### Numbering Rules
1. ADRs are numbered sequentially
2. Never reuse a number, even if an ADR is deprecated
3. Use leading zeros for consistency (e.g., ADR-001, not ADR-1)

## Creating a New ADR

### Step 1: Choose the Next Number
```bash
# Find the highest ADR number
ls /opt/jdbx/docs/adr/ADR-*.md | sort -V | tail -1
# Use the next sequential number
```

### Step 2: Use the Template
Create file: `/opt/jdbx/docs/adr/ADR-XXX-descriptive-name.md`

```markdown
# ADR-XXX: [Title]

**Date**: [YYYY-MM-DD]  
**Status**: Proposed | Accepted | Deprecated | Superseded  
**Version**: [First version where implemented]  
**Impact**: Critical | High | Medium | Low  

## Context
[What is the issue that we're seeing that is motivating this decision?]

## Decision
[What is the change that we're proposing/implementing?]

## Rationale
[Why is this the right decision? What are the principles behind it?]

## Implementation
[How will this be implemented? Key technical details]

## Consequences

### Positive
- [Positive outcomes]

### Negative
- [Negative outcomes]

### Neutral
- [Neutral observations]

## Technical Details
[Specific implementation details, file changes, etc.]

## Validation
[How do we know this works?]

## References
- Git commits: [commit hashes]
- Related ADRs: [ADR-XXX]
- Documentation: [links]
```

### Step 3: Update ADR-TIMELINE.md

Add entry in chronological order:

```markdown
### 📊 **ADR-XXX: [Title]** ([Date])
**Status**: Accepted | **Impact**: [Level] | **Version**: [X.Y.Z]

**Decision**: [One-line summary]

**Context**: [Brief context]

**Key Points**:
- [Main implementation points]
- [Important changes]

**Git Commits**: `[hash]` - [commit message]

**[Full ADR →](ADR-XXX-descriptive-name.md)**

---
```

### Step 4: Update Dependencies

If your ADR depends on or affects others, update the dependency graph in ADR-TIMELINE.md.

## Maintaining the Timeline

### Regular Reviews
- Quarterly review of all ADRs
- Verify git commit references
- Update status (Proposed → Accepted, etc.)
- Check for missing decisions

### Timeline Sections
1. **Chronological Summary**: High-level version progression
2. **Timeline Details**: Detailed decision entries
3. **Decision Dependencies**: Mermaid graph
4. **Architectural Principles**: Extracted patterns

### Git Integration
Always reference git commits:
```bash
# Find relevant commits
git log --grep="[feature/fix name]" --oneline

# Get full commit info
git show [hash]
```

## ADR Status Lifecycle

```
Proposed → Accepted → Implemented
    ↓         ↓            ↓
Rejected  Superseded  Deprecated
```

- **Proposed**: Under discussion
- **Accepted**: Approved but not yet implemented
- **Implemented**: Accepted and in codebase
- **Superseded**: Replaced by another ADR
- **Deprecated**: No longer relevant
- **Rejected**: Not accepted

## Backfilling Historical Decisions

When documenting past decisions:

1. **Research Sources**:
   - Git commit history
   - CHANGELOG.md entries
   - Code comments
   - Issue/PR discussions

2. **Dating**:
   - Use the implementation date, not documentation date
   - Reference the version where first appeared

3. **Validation**:
   - Verify the decision is still in effect
   - Check if superseded by later decisions

## Cross-References

### In Code
```c
/* See ADR-028 for checkpoint memory architecture */
memory_checkpoint_t* cp = memory_checkpoint_create();
```

### In Documentation
```markdown
The checkpoint memory system (see [ADR-028](../adr/ADR-028-checkpoint-based-memory-manager.md)) provides...
```

### In Commits
```
feat: Implement checkpoint memory manager

Implements the architecture described in ADR-028.
See docs/adr/ADR-028-checkpoint-based-memory-manager.md
```

## Quality Checklist

Before finalizing an ADR:

- [ ] Numbered sequentially
- [ ] Status is accurate
- [ ] Impact level assessed
- [ ] Context clearly explains the problem
- [ ] Decision is concrete and actionable
- [ ] Consequences are balanced
- [ ] Technical details are specific
- [ ] Git commits are referenced
- [ ] Added to ADR-TIMELINE.md
- [ ] Dependencies updated if needed

## Common Patterns

### Architectural Themes in JDBX
1. **Single Source of Truth**: Eliminate duplicates
2. **Memory Safety**: Checkpoint-based management
3. **Performance**: Lock-free where possible
4. **Security**: No hardcoded values
5. **Maintainability**: Clear boundaries

### Decision Categories
- 🏗️ **ARCHITECTURE**: System design
- 🔒 **MEMORY**: Memory management
- 🔐 **SECURITY**: Auth and security
- 🚀 **PERFORMANCE**: Optimizations
- 📚 **DOCUMENTATION**: Standards
- 🐛 **CRITICAL FIX**: Bug fixes
- 🎯 **INTEGRATION**: External systems
- ⚡ **FEATURES**: New capabilities

## Tools and Scripts

### Find Undocumented Decisions
```bash
# Find major commits without ADRs
git log --grep="BREAKING\|CRITICAL\|MAJOR" --oneline | \
  while read commit; do
    hash=$(echo $commit | cut -d' ' -f1)
    if ! grep -q "$hash" docs/adr/ADR-*.md; then
      echo "Undocumented: $commit"
    fi
  done
```

### Generate ADR Index
```bash
# List all ADRs with status
for f in docs/adr/ADR-[0-9]*.md; do
  echo "$(basename $f): $(grep "^**Status**:" $f | cut -d' ' -f2-)"
done
```

## Review Process

1. **Author** creates ADR as "Proposed"
2. **Team** reviews and discusses
3. **Decision** to accept/reject
4. **Implementation** if accepted
5. **Documentation** updates
6. **Timeline** maintenance

## Anti-Patterns to Avoid

❌ **Don't**:
- Create ADRs retroactively without research
- Skip timeline updates
- Ignore git history
- Use vague descriptions
- Forget cross-references
- Mix multiple decisions in one ADR

✅ **Do**:
- One decision per ADR
- Reference specific commits
- Update timeline immediately
- Keep technical details precise
- Link related ADRs
- Maintain consistent format

## Questions?

For questions about ADR maintenance:
1. Check existing ADRs for examples
2. Review git history for patterns
3. Consult CLAUDE.md for project principles
4. Follow single source of truth principle

---

Remember: ADRs are living documents that capture our architectural journey. Keep them accurate, maintainable, and useful for future developers.