/**
 * Optimized static file serving using sendfile()
 * 
 * This implementation uses the sendfile() system call to avoid
 * copying file data through userspace, significantly improving
 * performance for static file serving.
 */

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/tcp.h>
#include "core/server.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* Get MIME type from file extension */
static const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".woff") == 0) return "font/woff";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".ttf") == 0) return "font/ttf";
    
    return "application/octet-stream";
}

/* Serve static file using sendfile */
int serve_static_file_optimized(int client_fd, const char* file_path, http_request_t* request) {
    struct stat file_stat;
    int file_fd = -1;
    int result = -1;
    
    if (g_logger) {
        LOG_DEBUG("[SENDFILE] Serving static file: %s", file_path);
    }
    
    /* Open the file */
    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        if (g_logger) {
            LOG_ERROR("[SENDFILE] Failed to open file %s: %s", file_path, strerror(errno));
        }
        goto cleanup;
    }
    
    /* Get file statistics */
    if (fstat(file_fd, &file_stat) < 0) {
        if (g_logger) {
            LOG_ERROR("[SENDFILE] Failed to stat file %s: %s", file_path, strerror(errno));
        }
        goto cleanup;
    }
    
    /* Check if it's a regular file */
    if (!S_ISREG(file_stat.st_mode)) {
        if (g_logger) {
            LOG_ERROR("[SENDFILE] Not a regular file: %s", file_path);
        }
        goto cleanup;
    }
    
    /* Get MIME type */
    const char* mime_type = get_mime_type(file_path);
    
    /* Build HTTP response headers */
    char headers[4096];
    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: public, max-age=3600\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type,
        (long)file_stat.st_size
    );
    
    /* Apply CORS headers if needed */
    if (request && request->origin) {
        header_len = snprintf(headers, sizeof(headers),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Cache-Control: public, max-age=3600\r\n"
            "Access-Control-Allow-Origin: %s\r\n"
            "Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\n"
            "Connection: close\r\n"
            "\r\n",
            mime_type,
            (long)file_stat.st_size,
            request->origin
        );
    }
    
    /* Enable TCP_CORK to batch header and file data */
    int cork = 1;
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork)) < 0) {
        if (g_logger) {
            LOG_DEBUG("[SENDFILE] Failed to set TCP_CORK: %s", strerror(errno));
        }
    }
    
    /* Send headers */
    size_t bytes_sent = 0;
    while (bytes_sent < (size_t)header_len) {
        ssize_t sent = send(client_fd, headers + bytes_sent, header_len - bytes_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (g_logger) {
                LOG_ERROR("[SENDFILE] Failed to send headers: %s", strerror(errno));
            }
            goto cleanup;
        }
        bytes_sent += sent;
    }
    
    /* Send file using sendfile */
    off_t offset = 0;
    size_t remaining = file_stat.st_size;
    
    while (remaining > 0) {
        ssize_t sent = sendfile(client_fd, file_fd, &offset, remaining);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket buffer full, wait a bit */
                usleep(1000);
                continue;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                /* Client disconnected */
                if (g_logger) {
                    LOG_DEBUG("[SENDFILE] Client disconnected while sending %s", file_path);
                }
                goto cleanup;
            } else {
                if (g_logger) {
                    LOG_ERROR("[SENDFILE] sendfile failed for %s: %s", file_path, strerror(errno));
                }
                goto cleanup;
            }
        } else if (sent == 0) {
            /* No more data to send */
            break;
        }
        
        remaining -= sent;
        
        if (g_logger && (remaining == 0 || (file_stat.st_size - remaining) % (1024 * 1024) == 0)) {
            LOG_DEBUG("[SENDFILE] Progress for %s: %ld/%ld bytes sent", 
                     file_path, (long)(file_stat.st_size - remaining), (long)file_stat.st_size);
        }
    }
    
    /* Disable TCP_CORK to flush any remaining data */
    cork = 0;
    setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));
    
    if (g_logger) {
        LOG_DEBUG("[SENDFILE] Successfully served %s (%ld bytes)", file_path, (long)file_stat.st_size);
    }
    
    result = 0;
    
cleanup:
    if (file_fd >= 0) {
        close(file_fd);
    }
    return result;
}

/* Check if a request is for a static file */
int is_static_file_request(const char* path) {
    /* Check if path starts with /static/ or ends with common static file extensions */
    if (strncmp(path, "/static/", 8) == 0) return 1;
    if (strncmp(path, "/css/", 5) == 0) return 1;
    if (strncmp(path, "/js/", 4) == 0) return 1;
    if (strncmp(path, "/images/", 8) == 0) return 1;
    
    /* Check file extensions */
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0 ||
            strcmp(ext, ".css") == 0 ||
            strcmp(ext, ".js") == 0 ||
            strcmp(ext, ".png") == 0 ||
            strcmp(ext, ".jpg") == 0 ||
            strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".gif") == 0 ||
            strcmp(ext, ".svg") == 0 ||
            strcmp(ext, ".ico") == 0 ||
            strcmp(ext, ".woff") == 0 ||
            strcmp(ext, ".woff2") == 0 ||
            strcmp(ext, ".ttf") == 0) {
            return 1;
        }
    }
    
    return 0;
}