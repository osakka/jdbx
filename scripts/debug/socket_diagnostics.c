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
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/resource.h> /* For getrlimit */

/* Utility to diagnose socket binding issues */

/* Print a separator */
void print_separator(const char* title) {
    printf("\n=== %s ===\n", title);
}

/* List all network interfaces */
void list_network_interfaces() {
    print_separator("Network Interfaces");
    
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }
    
    printf("%-15s %-10s %-40s %s\n", "Interface", "Family", "Address", "Flags");
    printf("%-15s %-10s %-40s %s\n", "---------", "------", "-------", "-----");
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        
        family = ifa->ifa_addr->sa_family;
        
        /* For AF_INET* addresses */
        if (family == AF_INET || family == AF_INET6) {
            s = getnameinfo(ifa->ifa_addr,
                          (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                               sizeof(struct sockaddr_in6),
                          host, NI_MAXHOST,
                          NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }
            
            printf("%-15s %-10s %-40s %s%s%s%s\n",
                  ifa->ifa_name,
                  family == AF_INET ? "IPv4" : "IPv6",
                  host,
                  (ifa->ifa_flags & IFF_UP) ? "UP " : "",
                  (ifa->ifa_flags & IFF_LOOPBACK) ? "LOOPBACK " : "",
                  (ifa->ifa_flags & IFF_RUNNING) ? "RUNNING " : "",
                  (ifa->ifa_flags & IFF_MULTICAST) ? "MULTICAST " : "");
        }
    }
    
    freeifaddrs(ifaddr);
}

/* Check if a port is in use */
void check_port_usage(int port) {
    print_separator("Port Usage Check");
    
    int sockfd;
    struct sockaddr_in addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket: %s\n", strerror(errno));
        return;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    /* Try to bind to the port */
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Port %d is IN USE or UNAVAILABLE: %s\n", port, strerror(errno));
        
        if (errno == EADDRINUSE) {
            printf("Issue: Address already in use (EADDRINUSE).\n");
            printf("Another process is likely binding to port %d.\n", port);
            printf("Diagnostic: Run 'netstat -tulpn | grep %d' to find the process.\n", port);
        } else if (errno == EACCES) {
            printf("Issue: Permission denied (EACCES).\n");
            printf("Ports below 1024 require root privileges.\n");
        } else {
            printf("Issue: %s (errno=%d)\n", strerror(errno), errno);
        }
    } else {
        printf("Port %d is AVAILABLE and bindable.\n", port);
        
        /* Try to listen on the socket */
        if (listen(sockfd, 1) < 0) {
            printf("However, cannot listen on port %d: %s\n", port, strerror(errno));
        } else {
            printf("Socket can successfully listen on port %d.\n", port);
        }
    }
    
    close(sockfd);
}

/* Try hostname resolution */
void test_hostname_resolution(const char* hostname) {
    print_separator("Hostname Resolution");
    
    if (!hostname || strlen(hostname) == 0) {
        printf("No hostname provided for resolution test.\n");
        return;
    }
    
    printf("Resolving hostname: %s\n", hostname);
    
    struct hostent *he;
    struct in_addr **addr_list;
    
    if ((he = gethostbyname(hostname)) == NULL) {
        printf("Failed to resolve hostname '%s': %s\n", hostname, hstrerror(h_errno));
        
        switch (h_errno) {
            case HOST_NOT_FOUND:
                printf("Issue: Host not found in DNS. Check DNS configuration.\n");
                break;
            case NO_DATA:
                printf("Issue: Name valid but no IP address defined.\n");
                break;
            case NO_RECOVERY:
                printf("Issue: Non-recoverable DNS server error.\n");
                break;
            case TRY_AGAIN:
                printf("Issue: Temporary DNS error on server. Try again later.\n");
                break;
            default:
                printf("Issue: Unknown resolution error.\n");
        }
    } else {
        printf("Hostname resolution successful.\n");
        
        printf("Canonical name: %s\n", he->h_name);
        
        printf("IP addresses:\n");
        addr_list = (struct in_addr **) he->h_addr_list;
        for (int i = 0; addr_list[i] != NULL; i++) {
            printf("  %s\n", inet_ntoa(*addr_list[i]));
        }
    }
}

/* Test socket creation and options */
void test_socket_creation() {
    print_separator("Socket Creation Test");
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Failed to create socket: %s (errno=%d)\n", strerror(errno), errno);
        
        if (errno == EMFILE) {
            printf("Issue: Too many open file descriptors for the process.\n");
            printf("Diagnostic: Check with 'ulimit -n' and consider increasing limit.\n");
        } else if (errno == ENFILE) {
            printf("Issue: Too many open files in the system.\n");
            printf("Diagnostic: System-wide limit reached. Check '/proc/sys/fs/file-max'.\n");
        } else {
            printf("Issue: Unexpected error creating socket.\n");
        }
        
        return;
    }
    
    printf("Socket created successfully (fd=%d).\n", sockfd);
    
    /* Test SO_REUSEADDR */
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        printf("Failed to set SO_REUSEADDR: %s\n", strerror(errno));
    } else {
        printf("Successfully set SO_REUSEADDR option.\n");
    }
    
    /* Test socket descriptor limits */
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        printf("File descriptor limits: soft=%lu, hard=%lu\n", 
               (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }
    
    /* Get socket type */
    int socket_type;
    socklen_t socket_type_len = sizeof(socket_type);
    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &socket_type, &socket_type_len) == 0) {
        printf("Socket type: %s\n", 
               (socket_type == SOCK_STREAM) ? "SOCK_STREAM" : 
               (socket_type == SOCK_DGRAM) ? "SOCK_DGRAM" : "UNKNOWN");
    }
    
    close(sockfd);
}

/* Main diagnostic function */
int main(int argc, char* argv[]) {
    int port = 5000;
    char* hostname = "localhost";
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--host=", 7) == 0) {
            hostname = argv[i] + 7;
        }
    }
    
    printf("JSONdb Socket Diagnostics Tool\n");
    printf("-----------------------------\n");
    printf("Testing with Port: %d\n", port);
    printf("Testing with Host: %s\n", hostname);
    
    /* Run all diagnostics */
    list_network_interfaces();
    test_socket_creation();
    check_port_usage(port);
    test_hostname_resolution(hostname);
    
    /* Summary */
    print_separator("Summary");
    printf("Socket diagnostics completed. Check the above results for potential issues.\n");
    printf("If you see 'Address already in use' errors, ensure no other process is using port %d.\n", port);
    printf("For detailed process listing: netstat -tulpn | grep %d\n", port);
    
    return 0;
}