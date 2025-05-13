# Code Cleanup and Organization

This document outlines issues identified in the JSONdb codebase and proposes solutions to improve code organization, maintainability, and reliability.

## 1. Configuration Management Issues

### Current Problems

1. **Fragmented Configuration**:
   - Configuration defaults are spread across multiple files
   - Hardcoded values appear in several components
   - No single source of truth for default values

2. **Path Handling Inconsistencies**:
   - Mixture of absolute and relative paths
   - Inconsistent path resolution logic
   - Hardcoded paths in various components

3. **Component-specific Configurations**:
   - Many components define their own defaults
   - Duplicated logic for path handling and resolution
   - No standardized approach to component configuration

### Solution Plan

#### Phase 1: Centralized Configuration Defaults

1. Create a centralized `config_defaults.h` containing all default values
2. Replace hardcoded values in components with references to these defaults
3. Document all configuration options in a single location

#### Phase 2: Unified Path Resolution

1. Implement a consistent path resolution mechanism
2. Support both absolute and relative paths
3. Ensure all paths are resolved relative to a well-defined base directory

#### Phase 3: Component Configuration Integration

1. Update all components to use the centralized configuration system
2. Create component-specific configuration sections in the main config
3. Provide clean APIs for components to access their configuration

## 2. Implementation Plan

### Step 1: Create `config_defaults.h`

Create a new header file with all default configuration values.

### Step 2: Update `config_loader.c`

Modify the configuration loader to use the centralized defaults and implement better path resolution.

### Step 3: Component Updates

Update each component to reference the centralized defaults instead of defining their own.

### Step 4: Configuration Documentation

Create comprehensive documentation for all configuration options.

## 3. Benefits

1. **Single Source of Truth**: All defaults defined in one location
2. **Improved Maintainability**: Changes to defaults only need to be made in one place
3. **Better Organization**: Clear separation between code and configuration
4. **Consistent Behavior**: Standardized path resolution and configuration loading
5. **Easier Updates**: Simplified process for adding or modifying configuration options

## 4. Future Considerations

1. **Environment Variable Support**: Add support for configuration via environment variables
2. **Configuration Validation**: Implement validation for configuration values
3. **Hot Reload**: Support reloading configuration without restarting the server
4. **Configuration API**: Provide a REST API for viewing and updating configuration