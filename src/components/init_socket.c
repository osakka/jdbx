#include "init.h"
#include "core/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>

/* Initialize server socket - CLEAN IMPLEMENTATION based on proven approach */
init_status_t init_socket(server_config_t* config) {
    if (!config) {
        INIT_LOG_FAILURE("SOCKET", "NULL server configuration");
        return INIT_SOCKET_ERROR;
    }

    /* Log current process context */
    pid_t process_pid = getpid();
    INIT_LOG_DEBUG("SOCKET", "Socket initialization started in PID %d", process_pid);

    /* Create socket in blocking mode */
    INIT_LOG_PROGRESS("SOCKET", "Creating socket on %s:%d",
                    config->host ? config->host : "0.0.0.0", config->port);
                    
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to create socket: %s (errno=%d)", 
                       strerror(errno), errno);
        return INIT_SOCKET_ERROR;
    }

    INIT_LOG_SUCCESS("SOCKET", "Socket created (fd=%d)", socket_fd);

    /* Set socket options */
    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to set SO_REUSEADDR: %s", strerror(errno));
        /* Continue anyway, this is not fatal */
    } else {
        INIT_LOG_SUCCESS("SOCKET", "Socket option SO_REUSEADDR set");
    }

    /* Prepare address structure */
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(config->port);

    /* Handle host address - improved approach with proper error handling */
    if (!config->host || strlen(config->host) == 0 || strcmp(config->host, "0.0.0.0") == 0) {
        /* Bind to any address */
        address.sin_addr.s_addr = INADDR_ANY;
        INIT_LOG_PROGRESS("SOCKET", "Using INADDR_ANY (0.0.0.0) for binding");
    } else if (strcmp(config->host, "127.0.0.1") == 0 || strcmp(config->host, "localhost") == 0) {
        /* Bind to localhost */
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
        INIT_LOG_PROGRESS("SOCKET", "Using localhost (127.0.0.1) for binding");
    } else if (inet_addr(config->host) != INADDR_NONE) {
        /* It's a valid IP address */
        address.sin_addr.s_addr = inet_addr(config->host);
        INIT_LOG_PROGRESS("SOCKET", "Using IP address %s for binding", config->host);
    } else {
        /* It's a hostname that needs to be resolved - this might be problematic */
        if (g_logger) {
            LOG_WARNING("[INIT:SOCKET] Hostname '%s' provided, using INADDR_ANY (0.0.0.0) for binding", 
                       config->host);
        } else {
            fprintf(stderr, "[INIT:SOCKET] WARNING: Hostname '%s' provided, using INADDR_ANY (0.0.0.0) for binding\n",
                   config->host);
        }
        address.sin_addr.s_addr = INADDR_ANY;
    }

    /* Bind socket */
    INIT_LOG_PROGRESS("SOCKET", "Binding socket to %s:%d", 
                   config->host ? config->host : "0.0.0.0", config->port);
                   
    if (bind(socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to bind socket: %s (errno=%d)", 
                       strerror(errno), errno);
        close(socket_fd);
        return INIT_SOCKET_ERROR;
    }

    INIT_LOG_SUCCESS("SOCKET", "Socket bound successfully");

    /* Set up to listen for connections */
    INIT_LOG_PROGRESS("SOCKET", "Setting socket to listen state");
    
    if (listen(socket_fd, 10) < 0) {
        INIT_LOG_FAILURE("SOCKET", "Failed to listen on socket: %s (errno=%d)", 
                       strerror(errno), errno);
        close(socket_fd);
        return INIT_SOCKET_ERROR;
    }

    INIT_LOG_SUCCESS("SOCKET", "Socket listening successfully");

    /* Verify socket state */
    int acceptconn = 0;
    socklen_t acceptconn_len = sizeof(acceptconn);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_ACCEPTCONN, &acceptconn, &acceptconn_len) < 0) {
        if (g_logger) {
            LOG_WARNING("[INIT:SOCKET] Failed to check SO_ACCEPTCONN: %s", strerror(errno));
        } else {
            fprintf(stderr, "[INIT:SOCKET] WARNING: Failed to check SO_ACCEPTCONN: %s\n", strerror(errno));
        }
    } else {
        if (g_logger) {
            LOG_INFO("[INIT:SOCKET] Socket listening state: %s", 
                   acceptconn ? "LISTENING" : "NOT LISTENING");
        } else {
            printf("[INIT:SOCKET] Socket listening state: %s\n", 
                  acceptconn ? "LISTENING" : "NOT LISTENING");
        }
        
        if (!acceptconn) {
            INIT_LOG_FAILURE("SOCKET", "Socket is not in listening state despite successful listen() call");
            close(socket_fd);
            return INIT_SOCKET_ERROR;
        }
    }

    /* Store socket descriptor in config */
    config->socket_fd = socket_fd;

    INIT_LOG_SUCCESS("SOCKET", "Socket initialization complete (socket_fd=%d, port=%d)", 
                   socket_fd, config->port);

    return INIT_OK;
}