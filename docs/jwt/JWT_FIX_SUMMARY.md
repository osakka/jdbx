# JWT Authentication Fix Summary

## Issue Description

Authentication was failing for protected API endpoints despite generating valid JWT tokens. After thorough investigation, we identified that the issue was in how the JWT secret was being managed:

- The API context was storing a reference to the JWT secret string from the server configuration, but not making a copy
- This could potentially lead to inconsistencies if the original string was modified
- Lack of proper debugging made it difficult to identify where the JWT secret was being used differently

## Changes Made

1. **API Context Creation**: Updated `api_create_context()` in `api.c` to:
   - Create a copy of the JWT secret using `strdup()` instead of storing a reference
   - Add proper error handling if the copy fails
   - Add logging to track the JWT secret being used

2. **Resource Cleanup**: Updated `api_free_context()` in `api.c` to:
   - Free the copied JWT secret when the API context is destroyed

3. **Logging Improvements**: Added detailed logging throughout the JWT operations:
   - In `main.c`, added logging when initializing the API context to show the JWT secret
   - In `api.c`, added logging when creating and encoding JWT tokens
   - Added debug output to show the generated token for debugging purposes

4. **JWT Secret Consistency**: The changes ensure that:
   - The same JWT secret is consistently used for both token generation and verification
   - Any changes to the original string in the server config won't affect the API context

## Additional Considerations

- The JWT creation in `jwt.c` doesn't actually use the secret parameter passed to `jwt_create()`. The secret is only used in `jwt_encode()` and `jwt_verify()`. This design could be improved.
- The admin authentication system uses a completely different token format than the JWT system used for regular API authentication. This could be harmonized in the future.

## Validation

The changes should be validated by:
1. Restarting the server with the fixed code
2. Using the login endpoint to generate a token
3. Using that token to access protected endpoints
4. Verifying that authentication now succeeds

## Future Improvements

1. Consider unifying the admin and regular API authentication systems
2. Fix the inconsistency in the JWT implementation where `jwt_create()` takes a `secret` parameter but doesn't use it
3. Add automated tests for the authentication flow to prevent regressions

Fix completed on May 13, 2025