# JSON Database Server TODOs

This document contains detailed analysis of issues identified in BUILD_STATUS.md along with specific action items to address them.

## 1. Authentication/Authorization Flow

**Analysis:**
- Authentication endpoints (`/api/auth/login`, `/api/auth/register`) work correctly
- JWT token generation and validation appears to be implemented
- Most endpoints return "Unauthorized" despite valid token
- Two issues identified:
  1. JWT secret management issue: Secret was stored as a reference, not a copy in API context
  2. JWT verification issue: Base64url encoding/decoding inconsistency in signature verification

**Action Items:**
- [x] Debug token validation in `api_authenticate_request` function
- [x] Add detailed logging to track token validation steps
- [x] Fix JWT secret consistency between token generation and validation:
  - [x] Ensure the JWT secret is properly passed from server configuration to the API context
  - [x] Make API context create a copy of the JWT secret using strdup()
  - [x] Free the JWT secret copy when destroying the API context
  - [x] Update logging to track JWT secret usage
- [x] Fix JWT signature verification:
  - [x] Identify the issue with base64url decoding in JWT verification
  - [x] Add missing `base64_url_decode` function to properly handle URL-safe encoding
  - [x] Create fixed JWT implementation in `jwt_fix.c`
  - [x] Document the issue and fix in JWT_VERIFICATION_FIX.md
- [ ] Replace current `jwt.c` with the fixed version
- [ ] Test authentication with the fixed JWT implementation
- [ ] Consider updating admin token system to use JWT for better consistency

## 2. JavaScript Engine Initialization

**Analysis:**
- JS routes are defined in `api.c`
- JS engine initialization fails when attempting to execute scripts
- Issue may be in `js_engine_init` or dependency loading
- Error message indicates the engine isn't properly initialized

**Action Items:**
- [ ] Debug JS engine initialization in `js_engine.c`
- [ ] Ensure QuickJS dependencies are correctly configured
- [ ] Add better error handling and diagnostics to JS initialization
- [ ] Create minimal JS test file to validate engine functionality
- [ ] Document JS engine requirements and limitations

## 3. Documentation vs. Reality Gap

**Analysis:**
- API documentation describes many features not yet fully implemented
- Port number is incorrect (8080 vs. 5000)
- Admin API endpoints are undocumented
- Transaction features are documented but commented out in code

**Action Items:**
- [x] Update documentation to use correct port (5000)
- [ ] Add implementation status indicators to all documented endpoints
- [ ] Document Admin API endpoints
- [ ] Add "Coming Soon" markers for planned but unimplemented features
- [ ] Create detailed examples that work with the current implementation

## 4. Placeholder Implementations

**Analysis:**
- Many route handlers use placeholder implementations returning static data
- RBAC routes (lines 768-835 in `api.c`) return fixed responses
- Config routes return static data
- These implementations don't connect to actual functionality

**Action Items:**
- [ ] Identify all placeholder implementations
- [ ] Prioritize which features need full implementation first
- [ ] Replace placeholder RBAC implementations with actual functionality
- [ ] Replace config placeholders with real config operations
- [ ] Add clear comments in code marking placeholder implementations

## 5. Transaction Support

**Analysis:**
- Transaction routes are defined but commented out (lines 147-159 in `api.c`)
- Transaction commit/rollback routes exist but may lack implementation
- Documentation presents transactions as a core feature
- Transaction visualization endpoints are commented out (lines 64-68)

**Action Items:**
- [ ] Assess transaction implementation status
- [ ] Implement basic transaction begin/commit/rollback functionality
- [ ] Update transaction route handlers in `api.c`
- [ ] Add transaction isolation level support
- [ ] Implement transaction visualization endpoints
- [ ] Update documentation to match actual capabilities

## 6. Port Mismatch in Documentation

**Analysis:**
- Documentation referenced port 8080 throughout
- Actual server runs on port 5000 by default
- Port is configurable via command line but defaults to 5000

**Action Items:**
- [x] Update all documentation references from 8080 to 5000
- [x] Update curl examples to use correct port (modified `/opt/jsondb/share/examples/curl_examples.sh`)
- [x] Update Python client example to use correct port (modified `/opt/jsondb/share/examples/client/python_client.py`)
- [x] Update visualization examples to use correct port (modified `/opt/jsondb/share/examples/visualization_examples.sh`)
- [ ] Add note about port configurability in main documentation

## 7. Collection and Document API Issues

**Analysis:**
- Routes are defined but return authorization errors
- Path parsing and URL parameter extraction may have issues
- Route patterns for collection and document operations need verification

**Action Items:**
- [ ] Debug authorization checks in collection route handlers
- [ ] Verify path parsing in document route handlers
- [ ] Test document CRUD operations with proper authentication
- [ ] Add detailed logging for path parameter extraction
- [ ] Create comprehensive test suite for collection/document operations

## 8. JWT Implementation Security

**Analysis:**
- Current JWT implementation uses a simplified HMAC-SHA256 that is not cryptographically secure
- It's using a DJB2 hash algorithm as a placeholder instead of actual SHA-256
- The implementation could be vulnerable to various attacks

**Action Items:**
- [ ] Replace the simplified HMAC-SHA256 with a proper cryptographic implementation (OpenSSL)
- [ ] Add proper validation for all JWT claims (issuer, audience, etc.)
- [ ] Improve error reporting for token validation failures
- [ ] Add token refresh capabilities
- [ ] Create comprehensive JWT security test suite

## Implementation Priority

1. Fix authentication/authorization flow (highest priority)
   - [x] JWT secret management fix
   - [x] JWT signature verification fix
   - [x] Apply the fixes and test - COMPLETED
2. Fix collection and document API issues (now that authentication works)
3. Fix JavaScript engine initialization (noted error in logs)
4. Update documentation to match reality
5. Address port mismatch in documentation (completed)
6. Fix compiler warnings throughout the codebase
   - [x] Unused function warnings
   - [x] Unused parameter warnings
   - [x] Format-truncation warnings
   - [x] Sign comparison warnings
   - [x] External library warnings
   - [x] Document fixes in COMPILER_WARNING_FIXES_2025.md
7. Complete placeholder implementations
8. Improve JWT implementation security
9. Implement transaction support (lowest priority)

## Approach

1. Focus on fixing core functionality first (auth, collections, documents)
2. Update documentation to accurately reflect current state
3. Add clear status indicators for work-in-progress features
4. Create comprehensive tests to validate fixes