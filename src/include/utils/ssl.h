#ifndef JDBX_SSL_H
#define JDBX_SSL_H

/**
 * @file ssl.h
 * @brief SSL/TLS support for the JDBX server
 *
 * This header provides SSL/TLS functionality for securing the JDBX server communications.
 * It wraps OpenSSL functionality and provides a simpler interface for the server.
 */

#include <stddef.h>

/**
 * @brief SSL context structure (opaque)
 */
typedef struct ssl_context_t ssl_context_t;

/**
 * @brief SSL connection structure (opaque)
 */
typedef struct ssl_connection_t ssl_connection_t;

/**
 * @brief Error codes for SSL operations
 */
typedef enum {
    SSL_SUCCESS = 0,          /**< Operation succeeded */
    SSL_ERROR_INIT,           /**< Failed to initialize SSL */
    SSL_ERROR_CERTIFICATE,    /**< Certificate error */
    SSL_ERROR_KEY,            /**< Private key error */
    SSL_ERROR_HANDSHAKE,      /**< Handshake error */
    SSL_ERROR_IO,             /**< I/O error */
    SSL_ERROR_EOF,            /**< End of file (connection closed by peer) */
    SSL_ERROR_MEMORY,         /**< Memory allocation error */
    SSL_ERROR_VERIFICATION,   /**< Certificate verification error */
    SSL_ERROR_INVALID_PARAM,  /**< Invalid parameter */
    SSL_ERROR_UNKNOWN         /**< Unknown error */
} ssl_error_t;

/**
 * @brief SSL configuration options
 */
typedef struct {
    const char *cert_file;            /**< Path to the certificate file (PEM format) */
    const char *key_file;             /**< Path to the private key file (PEM format) */
    const char *ca_file;              /**< Path to the CA certificate file (optional) */
    const char *cipher_list;          /**< List of allowed ciphers (optional) */
    int verify_peer;                  /**< Whether to verify peer certificates (0 = no, 1 = yes) */
    int ignore_unexpected_eof;        /**< Whether to ignore unexpected EOF (OpenSSL 3.x compatibility) */
    int verify_depth;                 /**< Depth for certificate verification */
} ssl_config_t;

/**
 * @brief Initialize the SSL library
 * 
 * This function must be called before any other SSL functions.
 * 
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_library_init(void);

/**
 * @brief Clean up the SSL library
 * 
 * This function should be called when SSL is no longer needed.
 */
void ssl_library_cleanup(void);

/**
 * @brief Create a new SSL context
 * 
 * @param config SSL configuration options
 * @param[out] ctx Pointer to receive the created SSL context
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_context_create(const ssl_config_t *config, ssl_context_t **ctx);

/**
 * @brief Free an SSL context
 * 
 * @param ctx SSL context to free
 */
void ssl_context_free(ssl_context_t *ctx);

/**
 * @brief Create a new SSL connection
 * 
 * @param ctx SSL context
 * @param fd Socket file descriptor
 * @param[out] conn Pointer to receive the created SSL connection
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_connection_create(ssl_context_t *ctx, int fd, ssl_connection_t **conn);

/**
 * @brief Free an SSL connection
 * 
 * @param conn SSL connection to free
 */
void ssl_connection_free(ssl_connection_t *conn);

/**
 * @brief Perform SSL handshake
 * 
 * @param conn SSL connection
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_handshake(ssl_connection_t *conn);

/**
 * @brief Read data from an SSL connection
 * 
 * @param conn SSL connection
 * @param buffer Buffer to store read data
 * @param size Size of the buffer
 * @param[out] bytes_read Number of bytes read
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_read(ssl_connection_t *conn, void *buffer, size_t size, size_t *bytes_read);

/**
 * @brief Write data to an SSL connection
 * 
 * @param conn SSL connection
 * @param data Data to write
 * @param size Size of the data
 * @param[out] bytes_written Number of bytes written
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_write(ssl_connection_t *conn, const void *data, size_t size, size_t *bytes_written);

/**
 * @brief Check if an SSL connection is established
 * 
 * @param conn SSL connection
 * @return 1 if connected, 0 otherwise
 */
int ssl_is_connected(ssl_connection_t *conn);

/**
 * @brief Get the last SSL error as a string
 * 
 * @param error Error code
 * @return Error message string
 */
const char *ssl_error_string(ssl_error_t error);

/**
 * @brief Get the peer certificate information
 * 
 * @param conn SSL connection
 * @param buffer Buffer to store certificate information
 * @param size Size of the buffer
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_get_peer_certificate_info(ssl_connection_t *conn, char *buffer, size_t size);

/**
 * @brief Set the SSL verification mode
 * 
 * @param ctx SSL context
 * @param verify_mode Verification mode (0 = none, 1 = verify peer)
 * @return SSL_SUCCESS on success, error code otherwise
 */
ssl_error_t ssl_set_verify_mode(ssl_context_t *ctx, int verify_mode);

#endif /* JDBX_SSL_H */