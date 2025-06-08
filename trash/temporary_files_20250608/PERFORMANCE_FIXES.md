# JSONdb Performance Fixes

## 1. Add SSL Session Caching (High Impact)

Add to `ssl_context_create()` in `src/components/utils/ssl.c`:

```c
/* Enable session caching for performance */
SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_SERVER);
SSL_CTX_sess_set_cache_size(ssl_ctx, 128); /* Cache up to 128 sessions */
SSL_CTX_set_timeout(ssl_ctx, 300); /* 5 minute session timeout */
```

## 2. Enable HTTP Keep-Alive

Add to response headers in `src/components/core/http_response.c`:

```c
if (keep_alive_enabled) {
    add_header(response, "Connection", "keep-alive");
    add_header(response, "Keep-Alive", "timeout=5, max=100");
}
```

## 3. Replace Global Mutex with Read-Write Lock

In `src/include/database/database.h`:
```c
typedef struct {
    pthread_rwlock_t rwlock;  /* Replace pthread_mutex_t lock */
    // ... rest of structure
} database_t;
```

In read operations:
```c
pthread_rwlock_rdlock(&db->rwlock);  /* Multiple readers allowed */
// ... read operations ...
pthread_rwlock_unlock(&db->rwlock);
```

In write operations:
```c
pthread_rwlock_wrlock(&db->rwlock);  /* Exclusive write lock */
// ... write operations ...
pthread_rwlock_unlock(&db->rwlock);
```

## 4. Reduce Lock Manager Timeout

In `src/components/database/lock_manager.c`:
```c
#define DEFAULT_LOCK_TIMEOUT 5  /* Reduced from 30 seconds */
```

## Expected Performance Improvements

1. **SSL Session Caching**: 50-70% reduction in SSL handshake overhead
2. **HTTP Keep-Alive**: 30-50% reduction in connection overhead
3. **Read-Write Locks**: 3-5x improvement in read concurrency
4. **Reduced Timeout**: Faster failure detection, less blocking

## Implementation Priority

1. SSL Session Caching (easiest, high impact)
2. Read-Write Locks (medium effort, high impact)
3. HTTP Keep-Alive (requires connection management changes)
4. Lock timeout (trivial change, medium impact)