#include "jsondb/utils/ssl.h"
#include "jsondb/utils/logger.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <string.h>
#include <stdlib.h>

/* OpenSSL structures wrapped by our opaque types */
struct ssl_context_t {
    SSL_CTX *ssl_ctx;
};

struct ssl_connection_t {
    SSL *ssl;
    int connected;
};

/* Global initialization flag */
static int g_ssl_initialized = 0;

/* Get the last OpenSSL error as a string */
static char *get_openssl_error(void) {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return strdup("No OpenSSL error");
    }
    
    char *error_string = malloc(256);
    if (!error_string) {
        return NULL;
    }
    
    ERR_error_string_n(err, error_string, 256);
    return error_string;
}

/* Initialize the SSL library */
ssl_error_t ssl_library_init(void) {
    if (g_ssl_initialized) {
        return SSL_SUCCESS;
    }
    
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    g_ssl_initialized = 1;
    LOG_INFO("SSL library initialized");
    
    return SSL_SUCCESS;
}

/* Clean up the SSL library */
void ssl_library_cleanup(void) {
    if (!g_ssl_initialized) {
        return;
    }
    
    ERR_free_strings();
    EVP_cleanup();
    
    g_ssl_initialized = 0;
    LOG_INFO("SSL library cleaned up");
}

/* Create a new SSL context */
ssl_error_t ssl_context_create(const ssl_config_t *config, ssl_context_t **ctx) {
    if (!g_ssl_initialized) {
        LOG_ERROR("SSL library not initialized");
        return SSL_ERROR_INIT;
    }
    
    if (!config || !ctx) {
        LOG_ERROR("Invalid SSL configuration or context pointer");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    *ctx = NULL;
    
    /* Create a new SSL context */
    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        char *error = get_openssl_error();
        LOG_ERROR("Failed to create SSL context: %s", error ? error : "Unknown error");
        free(error);
        return SSL_ERROR_INIT;
    }
    
    /* Set the certificate file */
    if (config->cert_file) {
        if (SSL_CTX_use_certificate_file(ssl_ctx, config->cert_file, SSL_FILETYPE_PEM) <= 0) {
            char *error = get_openssl_error();
            LOG_ERROR("Failed to load certificate file: %s", error ? error : "Unknown error");
            free(error);
            SSL_CTX_free(ssl_ctx);
            return SSL_ERROR_CERTIFICATE;
        }
        
        LOG_INFO("Loaded SSL certificate: %s", config->cert_file);
    }
    
    /* Set the private key file */
    if (config->key_file) {
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config->key_file, SSL_FILETYPE_PEM) <= 0) {
            char *error = get_openssl_error();
            LOG_ERROR("Failed to load private key file: %s", error ? error : "Unknown error");
            free(error);
            SSL_CTX_free(ssl_ctx);
            return SSL_ERROR_KEY;
        }
        
        /* Check if the private key matches the certificate */
        if (SSL_CTX_check_private_key(ssl_ctx) <= 0) {
            char *error = get_openssl_error();
            LOG_ERROR("Private key does not match certificate: %s", error ? error : "Unknown error");
            free(error);
            SSL_CTX_free(ssl_ctx);
            return SSL_ERROR_KEY;
        }
        
        LOG_INFO("Loaded SSL private key: %s", config->key_file);
    }
    
    /* Set the CA certificate file */
    if (config->ca_file) {
        if (!SSL_CTX_load_verify_locations(ssl_ctx, config->ca_file, NULL)) {
            char *error = get_openssl_error();
            LOG_ERROR("Failed to load CA file: %s", error ? error : "Unknown error");
            free(error);
            SSL_CTX_free(ssl_ctx);
            return SSL_ERROR_CERTIFICATE;
        }
        
        LOG_INFO("Loaded SSL CA certificate: %s", config->ca_file);
    }
    
    /* Set the cipher list */
    if (config->cipher_list) {
        if (!SSL_CTX_set_cipher_list(ssl_ctx, config->cipher_list)) {
            char *error = get_openssl_error();
            LOG_ERROR("Failed to set cipher list: %s", error ? error : "Unknown error");
            free(error);
            SSL_CTX_free(ssl_ctx);
            return SSL_ERROR_INIT;
        }
        
        LOG_INFO("Set SSL cipher list: %s", config->cipher_list);
    }
    
    /* Set verification mode */
    int verify_mode = config->verify_peer ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_NONE;
    SSL_CTX_set_verify(ssl_ctx, verify_mode, NULL);
    
    /* Set verification depth */
    if (config->verify_depth > 0) {
        SSL_CTX_set_verify_depth(ssl_ctx, config->verify_depth);
    }
    
    /* Allocate our wrapper context */
    ssl_context_t *new_ctx = malloc(sizeof(ssl_context_t));
    if (!new_ctx) {
        LOG_ERROR("Failed to allocate SSL context wrapper");
        SSL_CTX_free(ssl_ctx);
        return SSL_ERROR_MEMORY;
    }
    
    new_ctx->ssl_ctx = ssl_ctx;
    *ctx = new_ctx;
    
    LOG_INFO("SSL context created successfully");
    return SSL_SUCCESS;
}

/* Free an SSL context */
void ssl_context_free(ssl_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    if (ctx->ssl_ctx) {
        SSL_CTX_free(ctx->ssl_ctx);
    }
    
    free(ctx);
}

/* Create a new SSL connection */
ssl_error_t ssl_connection_create(ssl_context_t *ctx, int fd, ssl_connection_t **conn) {
    if (!ctx || !conn || fd < 0) {
        LOG_ERROR("Invalid SSL context, connection pointer, or file descriptor");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    *conn = NULL;
    
    /* Create a new SSL connection */
    SSL *ssl = SSL_new(ctx->ssl_ctx);
    if (!ssl) {
        char *error = get_openssl_error();
        LOG_ERROR("Failed to create SSL connection: %s", error ? error : "Unknown error");
        free(error);
        return SSL_ERROR_INIT;
    }
    
    /* Set the socket file descriptor */
    if (!SSL_set_fd(ssl, fd)) {
        char *error = get_openssl_error();
        LOG_ERROR("Failed to set SSL file descriptor: %s", error ? error : "Unknown error");
        free(error);
        SSL_free(ssl);
        return SSL_ERROR_IO;
    }
    
    /* Allocate our wrapper connection */
    ssl_connection_t *new_conn = malloc(sizeof(ssl_connection_t));
    if (!new_conn) {
        LOG_ERROR("Failed to allocate SSL connection wrapper");
        SSL_free(ssl);
        return SSL_ERROR_MEMORY;
    }
    
    new_conn->ssl = ssl;
    new_conn->connected = 0;
    *conn = new_conn;
    
    return SSL_SUCCESS;
}

/* Free an SSL connection */
void ssl_connection_free(ssl_connection_t *conn) {
    if (!conn) {
        return;
    }
    
    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
    }
    
    free(conn);
}

/* Perform SSL handshake */
ssl_error_t ssl_handshake(ssl_connection_t *conn) {
    if (!conn || !conn->ssl) {
        LOG_ERROR("Invalid SSL connection");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    /* Perform the handshake */
    int result = SSL_accept(conn->ssl);
    if (result <= 0) {
        int error = SSL_get_error(conn->ssl, result);
        char *error_str = get_openssl_error();
        
        LOG_ERROR("SSL handshake failed: %s (code: %d)", error_str ? error_str : "Unknown error", error);
        free(error_str);
        
        return SSL_ERROR_HANDSHAKE;
    }
    
    conn->connected = 1;
    LOG_INFO("SSL handshake completed successfully");
    
    return SSL_SUCCESS;
}

/* Read data from an SSL connection */
ssl_error_t ssl_read(ssl_connection_t *conn, void *buffer, size_t size, size_t *bytes_read) {
    if (!conn || !conn->ssl || !buffer || !bytes_read) {
        LOG_ERROR("Invalid SSL read parameters");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    if (!conn->connected) {
        LOG_ERROR("SSL connection not established");
        return SSL_ERROR_HANDSHAKE;
    }
    
    *bytes_read = 0;
    
    /* Read data */
    int result = SSL_read(conn->ssl, buffer, (int)size);
    if (result <= 0) {
        int error = SSL_get_error(conn->ssl, result);
        
        /* Handle non-fatal errors */
        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
            return SSL_SUCCESS;
        }
        
        /* Handle connection closed */
        if (error == SSL_ERROR_ZERO_RETURN) {
            conn->connected = 0;
            return SSL_SUCCESS;
        }
        
        char *error_str = get_openssl_error();
        LOG_ERROR("SSL read failed: %s (code: %d)", error_str ? error_str : "Unknown error", error);
        free(error_str);
        
        return SSL_ERROR_IO;
    }
    
    *bytes_read = (size_t)result;
    return SSL_SUCCESS;
}

/* Write data to an SSL connection */
ssl_error_t ssl_write(ssl_connection_t *conn, const void *data, size_t size, size_t *bytes_written) {
    if (!conn || !conn->ssl || !data || !bytes_written) {
        LOG_ERROR("Invalid SSL write parameters");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    if (!conn->connected) {
        LOG_ERROR("SSL connection not established");
        return SSL_ERROR_HANDSHAKE;
    }
    
    *bytes_written = 0;
    
    /* Write data */
    int result = SSL_write(conn->ssl, data, (int)size);
    if (result <= 0) {
        int error = SSL_get_error(conn->ssl, result);
        
        /* Handle non-fatal errors */
        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
            return SSL_SUCCESS;
        }
        
        char *error_str = get_openssl_error();
        LOG_ERROR("SSL write failed: %s (code: %d)", error_str ? error_str : "Unknown error", error);
        free(error_str);
        
        return SSL_ERROR_IO;
    }
    
    *bytes_written = (size_t)result;
    return SSL_SUCCESS;
}

/* Check if an SSL connection is established */
int ssl_is_connected(ssl_connection_t *conn) {
    if (!conn) {
        return 0;
    }
    
    return conn->connected;
}

/* Get the last SSL error as a string */
const char *ssl_error_string(ssl_error_t error) {
    switch (error) {
        case SSL_SUCCESS:
            return "Success";
        case SSL_ERROR_INIT:
            return "SSL initialization error";
        case SSL_ERROR_CERTIFICATE:
            return "Certificate error";
        case SSL_ERROR_KEY:
            return "Private key error";
        case SSL_ERROR_HANDSHAKE:
            return "Handshake error";
        case SSL_ERROR_IO:
            return "I/O error";
        case SSL_ERROR_MEMORY:
            return "Memory allocation error";
        case SSL_ERROR_VERIFICATION:
            return "Certificate verification error";
        case SSL_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case SSL_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

/* Get the peer certificate information */
ssl_error_t ssl_get_peer_certificate_info(ssl_connection_t *conn, char *buffer, size_t size) {
    if (!conn || !conn->ssl || !buffer || size == 0) {
        LOG_ERROR("Invalid parameters for getting peer certificate info");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    buffer[0] = '\0';
    
    X509 *cert = SSL_get_peer_certificate(conn->ssl);
    if (!cert) {
        strncpy(buffer, "No peer certificate", size - 1);
        buffer[size - 1] = '\0';
        return SSL_ERROR_VERIFICATION;
    }
    
    /* Get subject name */
    X509_NAME *subject_name = X509_get_subject_name(cert);
    if (subject_name) {
        char subject[256] = {0};
        X509_NAME_oneline(subject_name, subject, sizeof(subject) - 1);
        
        strncpy(buffer, "Subject: ", size - 1);
        buffer[size - 1] = '\0';
        
        strncat(buffer, subject, size - strlen(buffer) - 1);
    }
    
    /* Get issuer name */
    X509_NAME *issuer_name = X509_get_issuer_name(cert);
    if (issuer_name) {
        char issuer[256] = {0};
        X509_NAME_oneline(issuer_name, issuer, sizeof(issuer) - 1);
        
        strncat(buffer, "\nIssuer: ", size - strlen(buffer) - 1);
        strncat(buffer, issuer, size - strlen(buffer) - 1);
    }
    
    /* Free the certificate */
    X509_free(cert);
    
    return SSL_SUCCESS;
}

/* Set the SSL verification mode */
ssl_error_t ssl_set_verify_mode(ssl_context_t *ctx, int verify_mode) {
    if (!ctx || !ctx->ssl_ctx) {
        LOG_ERROR("Invalid SSL context");
        return SSL_ERROR_INVALID_PARAM;
    }
    
    int mode = verify_mode ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_NONE;
    SSL_CTX_set_verify(ctx->ssl_ctx, mode, NULL);
    
    LOG_INFO("SSL verify mode set to: %s", verify_mode ? "verify peer" : "none");
    
    return SSL_SUCCESS;
}