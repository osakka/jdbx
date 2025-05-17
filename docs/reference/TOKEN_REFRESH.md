# Token Refresh Mechanism

## Overview

JSONdb now implements a token refresh mechanism to improve security and user experience. This document describes how token refresh works and how to use it in your applications.

## How It Works

1. When a user logs in (`/api/auth/login`), they receive:
   - An access token (short-lived, 30 minutes)
   - A refresh token (long-lived, 7 days)
   - Expiration information for the access token

2. The client application should:
   - Store both tokens securely
   - Use the access token for all API requests
   - Track token expiration
   - Refresh the access token before it expires

3. When the access token is about to expire, the client can:
   - Send the refresh token to `/api/auth/refresh`
   - Receive a new access token and refresh token pair
   - Update stored tokens

4. If the refresh token is invalid or expired:
   - The server returns a 401 Unauthorized response
   - The client should redirect to the login page

## API Endpoints

### Login: `/api/auth/login`

**Request:**
```json
{
  "username": "user",
  "password": "password"
}
```

**Response:**
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "user_id": "user123",
  "username": "user",
  "expires_in": 1800
}
```

### Refresh Token: `/api/auth/refresh`

**Request:**
```json
{
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response:**
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...", // New access token
  "refresh_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...", // New refresh token
  "user_id": "user123",
  "username": "user",
  "expires_in": 1800
}
```

## Client-Side Implementation

The web client automatically handles token refresh:

1. The client stores tokens in localStorage:
   - `jsondb_auth_token`: The access token
   - `jsondb_refresh_token`: The refresh token
   - `jsondb_token_expires`: The expiration timestamp

2. Token validity checking:
   - Before making API requests, the client checks if the token is about to expire
   - If the token expires in less than 60 seconds, a refresh is triggered
   - The `fetchWithTokenRefresh()` utility handles automatic token refresh

3. Auto-refresh behavior:
   - If an API call returns 401 Unauthorized, a token refresh is attempted
   - If successful, the original request is retried with the new token
   - If unsuccessful, the user is directed to the login screen

## Security Considerations

- Refresh tokens have a longer lifetime and should be stored securely
- The server validates refresh tokens to ensure they haven't been tampered with
- Refresh tokens have a built-in expiration (7 days by default)
- Each refresh operation provides a completely new pair of tokens
- The token type is encoded in the JWT payload to prevent token misuse

## Implementation Details

1. JWT tokens include claims:
   - `sub`: User ID
   - `iss`: Issuer ("jsondb")
   - `exp`: Expiration timestamp
   - `iat`: Issued at timestamp
   - `type`: Token type ("access" or "refresh")
   - `username`: Username (access tokens only)

2. Access tokens expire after 30 minutes
3. Refresh tokens expire after 7 days
4. The `jwt_verify_refresh_token()` function validates refresh tokens and extracts the user ID