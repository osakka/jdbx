# OpenSSL 3.x EOF Issue - Comprehensive Solution

## The Problem

OpenSSL 3.x reintroduced strict EOF checking to prevent truncation attacks. This causes the error:
```
SSL Library Error: error:0A000126:SSL routines::unexpected eof while reading
```

The issue affects:
- curl 7.x/8.x with OpenSSL 3.x backend
- Python requests with OpenSSL 3.x
- Any client using OpenSSL 3.x that doesn't send proper SSL close_notify

## Root Cause

1. **BEAST Attack Mitigation**: OpenSSL implements 1/n-1 packet splitting (sending 1 byte, then n-1 bytes)
2. **Strict EOF Checking**: OpenSSL 3.x treats missing close_notify as an error
3. **Client Behavior**: Many clients close the TCP connection without sending SSL close_notify

## Solutions

### 1. Server-Side: Proper SSL_OP_IGNORE_UNEXPECTED_EOF Implementation

We already have this implemented correctly:
```c
SSL_CTX_set_options(ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
```

However, this only prevents OpenSSL from treating EOF as a fatal SSL error. It doesn't make missing application data appear.

### 2. Client-Side Solutions

#### For curl:
```bash
# Use HTTP/1.0 to avoid chunked encoding issues
curl --http1.0 -k https://localhost:5000/api/endpoint

# Or force connection close
curl -H "Connection: close" -k https://localhost:5000/api/endpoint

# Or use different SSL backend
curl --tls-max 1.2 -k https://localhost:5000/api/endpoint
```

#### For Python requests:
```python
import requests
from requests.adapters import HTTPAdapter
from urllib3.poolmanager import PoolManager
import ssl

class TLSAdapter(HTTPAdapter):
    def init_poolmanager(self, *args, **kwargs):
        ctx = ssl.create_default_context()
        ctx.set_ciphers('DEFAULT@SECLEVEL=1')
        ctx.options |= 0x4  # SSL_OP_LEGACY_SERVER_CONNECT
        kwargs['ssl_context'] = ctx
        return super().init_poolmanager(*args, **kwargs)

session = requests.Session()
session.mount('https://', TLSAdapter())
session.verify = False

# Add explicit Connection: close header
response = session.post(
    "https://localhost:5000/api/auth/login",
    json={"username": "admin", "password": "secure123456789"},
    headers={"Connection": "close"}
)
```

### 3. System-Wide OpenSSL Configuration

Create `/etc/ssl/openssl.cnf.d/ignore_eof.cnf`:
```ini
[system_default_sect]
Options = IgnoreUnexpectedEOF
```

Note: Many applications ignore this for security reasons.

### 4. The Real Fix: Proper HTTP Handling

The actual issue is that clients are not completing their HTTP requests properly. The correct solution is to ensure clients:

1. Send all data specified in Content-Length
2. Properly close SSL connections with close_notify
3. Handle chunked encoding correctly

## Testing Strategy

### 1. Test with HTTP (no SSL)
Temporarily disable SSL to isolate the issue:
```bash
# In jdbx.env
JDBX_USE_SSL=false

# Restart server
build/jdbx_runtime.sh restart

# Test without SSL
curl http://localhost:5000/api/auth/login -d '{"username":"admin","password":"secure123456789"}'
```

### 2. Use Alternative Clients
```bash
# Use wget instead of curl
wget --no-check-certificate --post-data='{"username":"admin","password":"secure123456789"}' \
     --header='Content-Type: application/json' \
     https://localhost:5000/api/auth/login

# Use httpie
http --verify=no POST https://localhost:5000/api/auth/login \
     username=admin password=secure123456789
```

### 3. Use Raw OpenSSL s_client
```bash
echo -e "POST /api/auth/login HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 52\r\nConnection: close\r\n\r\n{\"username\":\"admin\",\"password\":\"secure123456789\"}" | \
openssl s_client -connect localhost:5000 -ign_eof
```

## Recommendation

1. **For Development**: Use HTTP or implement the Python adapter above
2. **For Production**: Document the issue and provide client examples that work
3. **Long-term**: Wait for client libraries to fix their OpenSSL 3.x compatibility

## References

- https://github.com/openssl/openssl/issues/18866
- https://github.com/curl/curl/issues/7800
- https://github.com/php/php-src/issues/8369