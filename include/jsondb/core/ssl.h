#ifndef SSL_H
#define SSL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
/* SSL type definitions - mocked for build without OpenSSL */
#ifndef SSL_CTX
typedef void* SSL_CTX;
#endif
#ifndef SSL
typedef void* SSL;
#endif

/* SSL context */
typedef struct {
    SSL_CTX* ctx;
    SSL* ssl;
    const char* cert_path;
    const char* key_path;
} ssl_context_t;

/* SSL function prototypes */
int ssl_init();
void ssl_cleanup();
ssl_context_t* ssl_create_context(const char* cert_path, const char* key_path);
void ssl_free_context(ssl_context_t* context);
SSL* ssl_create_connection(ssl_context_t* context, int socket_fd);
void ssl_free_connection(SSL* ssl);
int ssl_read(SSL* ssl, char* buffer, int length);
int ssl_write(SSL* ssl, const char* buffer, int length);

#endif /* SSL_H */