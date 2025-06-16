# Source Code Organization

This document describes the current organization of the source code that establishes a single source of truth for the codebase.

## Summary

The source code is organized with the following key characteristics:

1. All implementations reside exclusively in `src/components/`.
2. All headers reside exclusively in `src/include/`.
3. The project builds successfully with this clear structure.
4. Documentation has been updated to reflect this organization.

## Project Structure

The code is organized as follows:

1. **Headers** in `src/include/`:
   - Organized by component (api, core, database, etc.)
   - Publicly accessible interfaces for all components
   - Clear organization with a consistent pattern

2. **Implementation** in `src/components/`:
   - Organized by component, matching the header organization
   - Self-contained implementation files
   - Single source of truth for each component

3. **JavaScript Resources**:
   - JavaScript utility files in `share/js/functions/`
   - Clearly separated from core C implementation

4. **Build System**:
   - Main Makefile in `src/Makefile`
   - Builds all components into a unified binary
   - Simple and consistent configuration

## Documentation Resources

The following documentation files describe the source code organization:

1. `docs/architecture/project_structure.md`: Detailed description of the directory structure.
2. `docs/architecture/include_path_updates.md`: Guidelines for include path usage.
3. `docs/architecture/component_organization.md`: Details on the component organization.

## Build and Development

The project builds successfully with this structure, and development follows a consistent pattern:

1. To build the entire project:
   ```
   cd src/
   make
   ```

2. To add a new component:
   - Add header file to `src/include/{component}/`
   - Add implementation file to `src/components/{component}/`
   - Update Makefile if needed

3. To modify an existing component:
   - Find the header in `src/include/{component}/`
   - Find the implementation in `src/components/{component}/`
   - Make changes while preserving the existing structure

## Conclusion

The source code organization provides a clean, maintainable codebase with a clear separation between headers and implementation and a single source of truth for each file. This structure supports ongoing development efforts and makes the codebase more accessible to new developers.