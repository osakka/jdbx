# UUID Migration Checklist

## Phase 1: Add Helper Functions
- [ ] Create `get_document_id()` helper in json_helpers.c
- [ ] Create `set_document_id()` helper in json_helpers.c
- [ ] Add helpers to json_helpers.h
- [ ] Test helper functions

## Phase 2: Update Core Database Operations
- [ ] Update database.c READ operations
- [ ] Update database.c WRITE operations
- [ ] Update operations.c for all CRUD operations
- [ ] Update indexed_document_operations.c
- [ ] Update batch_operations.c

## Phase 3: Update Binary Format
- [ ] Update binary_format.c serialization
- [ ] Update binary_format.c deserialization
- [ ] Ensure backward compatibility in file format

## Phase 4: Update API Layer
- [ ] Update api.c request handling
- [ ] Update api.c response generation
- [ ] Update authentication_handler.c
- [ ] Update session_api.c
- [ ] Update rbac_api.c

## Phase 5: Update Query System
- [ ] Update query_language.c to support both fields
- [ ] Update query optimizer
- [ ] Add query rewriting for _id -> uuid

## Phase 6: Update Schema System
- [ ] Update schema.c validation
- [ ] Update json_schema_manager.c
- [ ] Update system schemas to include uuid

## Phase 7: Update JavaScript Integration
- [ ] Update js_native_storage.c
- [ ] Update app.js frontend code
- [ ] Update example scripts
- [ ] Create migration utilities

## Phase 8: Update Tests
- [ ] Update all test files
- [ ] Create backward compatibility tests
- [ ] Create migration tests

## Phase 9: Documentation
- [ ] Update API documentation
- [ ] Create migration guide
- [ ] Update examples
- [ ] Update README files

## Phase 10: Final Testing
- [ ] Full regression testing
- [ ] Performance testing
- [ ] Backward compatibility testing
- [ ] Migration testing with existing data
