# JSONdb Documentation

This directory (`/doc`) is the authoritative source of truth for all project documentation.

## Documentation Organization

The documentation is organized into these directories:

- `/api/` - API documentation and usage guides
- `/architecture/` - Design and architecture documentation
- `/development/` - Developer documentation and build guides
- `/guidelines/` - Project guidelines and contribution rules
- `/guides/` - User guides and tutorials
- `/integration/` - Integration documentation and status
- `/reference/` - Reference materials and detailed information
- `/status/` - Project status and progress tracking

## Documentation Standards

- All documentation is written in Markdown format
- Documentation follows the "One source of truth" principle from `guidelines/CLAUDE.md`
- README.md files may exist in code directories for context-specific guidance
- All substantial documentation belongs in the `/doc` directory

## Cross-References

When referring to other documents, use relative paths from the document location:

```markdown
Please see the [API documentation](../api/API.md) for details.
```

## Maintaining Documentation

- Keep documentation updated as code changes
- Follow the principles in `guidelines/CLAUDE.md`
- Document thoroughly with clear, concise language
- Update index files when adding new documentation