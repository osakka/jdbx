# UUID Migration Scripts

This directory contains three scripts to help with the systematic migration from `_id` to `uuid` fields while maintaining backward compatibility.

## Scripts Overview

### 1. `uuid_migration_helper.sh`
**Purpose**: Analyzes the entire codebase to find all `_id` references and categorizes them.

**Features**:
- Scans all C, header, JavaScript, and HTML files
- Creates detailed analysis files for each file containing `_id`
- Categorizes patterns into READ, WRITE, QUERY, and SCHEMA types
- Generates migration templates and a comprehensive checklist
- Creates a summary report of all files requiring attention

**Usage**:
```bash
./uuid_migration_helper.sh
```

**Output**: Creates `/opt/jdbx/uuid_migration_analysis/` directory with:
- Individual analysis files for each source file
- `migration_templates.txt` - Code templates for each pattern type
- `migration_summary.txt` - Summary report of all files
- `migration_checklist.md` - Step-by-step migration checklist

### 2. `uuid_migration_updater.sh`
**Purpose**: Interactive tool for analyzing and updating files with guided assistance.

**Features**:
- Interactive menu system
- Shows code context with line numbers
- Suggests replacements based on pattern type
- Creates helper functions for UUID migration
- Shows migration statistics
- Analyzes specific files interactively

**Usage**:
```bash
./uuid_migration_updater.sh
```

**Menu Options**:
1. Run full analysis
2. Show migration statistics
3. Create helper functions (uuid_helpers.c/h)
4. Analyze specific file interactively
5. Show migration templates
6. Exit

### 3. `uuid_migration_auto_update.sh`
**Purpose**: Automatically updates common patterns in the codebase.

**Features**:
- Creates automatic backup before making changes
- Updates C read patterns (json_object_get)
- Updates C write patterns (json_object_set)
- Updates C query patterns (strcmp)
- Updates JavaScript patterns
- Can process specific components or all files

**Usage**:
```bash
./uuid_migration_auto_update.sh
```

**Update Options**:
1. Database components only
2. API components only
3. RBAC components only
4. JavaScript files only
5. Test files only
6. All components (recommended)
7. Specific file

## Migration Strategy

### Phase 1: Analysis and Preparation
1. Run `uuid_migration_helper.sh` to analyze the codebase
2. Review the generated analysis files and summary
3. Use `uuid_migration_updater.sh` to create helper functions

### Phase 2: Manual Review
1. Use `uuid_migration_updater.sh` option 4 to interactively review critical files
2. Understand the context of each `_id` usage
3. Identify any special cases that need manual attention

### Phase 3: Automated Updates
1. Create a backup of your codebase
2. Run `uuid_migration_auto_update.sh` to automatically update common patterns
3. Review changes with `git diff`
4. Compile and test after each component update

### Phase 4: Testing and Validation
1. Compile the updated code
2. Run the test suite
3. Test backward compatibility with existing data
4. Test new UUID functionality

## Important Notes

1. **Backward Compatibility**: All updates maintain backward compatibility by:
   - Reading: Check `uuid` first, fall back to `_id`
   - Writing: Write both `uuid` and `_id` with the same value
   - Querying: Check both `uuid` and `_id` fields

2. **Helper Functions**: The created helper functions provide a consistent interface:
   - `get_document_id()` - Get ID with automatic fallback
   - `set_document_id()` - Set both uuid and _id
   - `is_document_id_field()` - Check if field is an ID field
   - `migrate_id_to_uuid()` - Migrate existing documents

3. **Backup**: Always backup before running automated updates:
   - `uuid_migration_auto_update.sh` creates automatic backups
   - Backups are timestamped in `/opt/jdbx/uuid_migration_backup_*`

4. **Testing**: After updates:
   - Ensure all code compiles without warnings
   - Run existing tests
   - Create new tests for UUID functionality
   - Test data migration scenarios

## Example Migration Pattern

### Before:
```c
json_t *id = json_object_get(doc, "_id");
```

### After:
```c
json_t *id = json_object_get(doc, "uuid");
if (!id) {
    id = json_object_get(doc, "_id");  // Backward compatibility
}
```

Or using helper functions:
```c
const char *id = get_document_id(doc);
```

## Troubleshooting

- If automated updates cause issues, restore from the backup directory
- For complex patterns, use the interactive updater for manual review
- Check compilation errors after updates to identify edge cases
- Use `git diff` to review all changes before committing