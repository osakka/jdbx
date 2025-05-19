# JSONdb Documentation

This directory (`/docs`) is the authoritative source of truth for all project documentation.

## Documentation Organization

The documentation is organized into these directories:

- `/api/` - API documentation and usage guides
- `/architecture/` - Design and architecture documentation
- `/build/` - Build-related documentation
- `/compiler/` - Compiler warning fixes and related information
- `/development/` - Developer documentation and build guides
- `/guidelines/` - Project guidelines and contribution rules
- `/guides/` - User guides and tutorials
- `/implementation/` - Implementation status and details
- `/integration/` - Integration documentation and status
- `/jwt/` - JWT verification and authentication details
- `/port/` - Port and network binding information
- `/rbac/` - Role-based access control documentation
- `/reference/` - Reference materials and detailed information
- `/socket-binding/` - Socket binding and network connection documentation
- `/src/` - Source code documentation
- `/status/` - Project status and progress tracking
- `/testing/` - Testing framework and approach
- `/threads/` - Thread management and synchronization
- `/tty/` - TTY and terminal handling documentation

## Documentation Standards

- All documentation is written in Markdown format
- Documentation follows the "One source of truth" principle from `guidelines/CLAUDE.md`
- README.md files may exist in code directories for context-specific guidance
- All substantial documentation belongs in the `/docs` directory

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