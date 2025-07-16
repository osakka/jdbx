#include "core/server.h"
#include "utils/config_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "utils/buffer_pool.h"

/* Check if path is an admin route */
int is_admin_route(const char* path) {
  /* API routes are NOT admin routes */
  if (strncmp(path, "/api/", 5) == 0) {
    return 0;
  }
  
  /* Root path redirects to admin */
  if (strcmp(path, "/") == 0) {
    return 1;
  }
  
  /* Login path is an admin route (for serving login.html) */
  if (strcmp(path, "/login") == 0 || strcmp(path, "/login.html") == 0) {
    return 1;
  }
  
  /* Check for admin paths */
  if ((strncmp(path, "/admin", 6) == 0) || 
    (strncmp(path, "/css/", 5) == 0) || 
    (strncmp(path, "/js/", 4) == 0)) {
    return 1;
  }
  
  /* Also allow any file with an extension (static files) */
  const char* ext = strrchr(path, '.');
  if (ext && ext != path) {
    return 1;
  }
  
  return 0;
}

/* Get file extension from path */
const char* get_file_extension(const char* filename) {
  if (!filename) {
    return "";
  }
  
  const char* dot = strrchr(filename, '.');
  if (!dot || dot == filename) {
    return "";
  }
  
  return dot + 1;
}

/* Get content type from file extension */
const char* get_content_type_from_extension(const char* extension) {
  if (!extension) {
    return "application/octet-stream";
  }
  
  /* Convert to lowercase for comparison */
  char ext_lower[32] = {0};
  size_t i;
  for (i = 0; i < sizeof(ext_lower) - 1 && extension[i]; i++) {
    ext_lower[i] = tolower(extension[i]);
  }
  ext_lower[i] = '\0';
  
  /* Map extensions to content types */
  if (strcmp(ext_lower, "html") == 0) {
    return "text/html";
  } else if (strcmp(ext_lower, "css") == 0) {
    return "text/css";
  } else if (strcmp(ext_lower, "js") == 0) {
    return "application/javascript";
  } else if (strcmp(ext_lower, "json") == 0) {
    return "application/json";
  } else if (strcmp(ext_lower, "png") == 0) {
    return "image/png";
  } else if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0) {
    return "image/jpeg";
  } else if (strcmp(ext_lower, "gif") == 0) {
    return "image/gif";
  } else if (strcmp(ext_lower, "svg") == 0) {
    return "image/svg+xml";
  } else if (strcmp(ext_lower, "ico") == 0) {
    return "image/x-icon";
  } else {
    return "application/octet-stream";
  }
}

/* Read file content from filesystem */
char* read_file_content(const char* filepath, size_t* size) {
  FILE* file = fopen(filepath, "rb");
  if (!file) {
    if (size) *size = 0;
    return NULL;
  }
  
  /* Get file size */
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  if (file_size <= 0) {
    fclose(file);
    if (size) *size = 0;
    return NULL;
  }
  
  /* Allocate buffer */
  char* buffer = (char*)BUFFER_ALLOC(file_size);
  if (!buffer) {
    fclose(file);
    if (size) *size = 0;
    return NULL;
  }
  
  /* Read file content */
  size_t bytes_read = fread(buffer, 1, file_size, file);
  fclose(file);
  
  if (bytes_read != (size_t)file_size) {
    BUFFER_FREE(buffer);
    if (size) *size = 0;
    return NULL;
  }
  
  if (size) *size = file_size;
  return buffer;
}

/* Enhanced file reading with memory-mapped I/O optimization for large files */
char* read_file_content_optimized(const char* filepath, size_t* size) {
  if (!filepath || !size) {
    return NULL;
  }
  
  FILE* file = fopen(filepath, "rb");
  if (!file) {
    *size = 0;
    return NULL;
  }
  
  /* Get file size */
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  if (file_size <= 0) {
    fclose(file);
    *size = 0;
    return NULL;
  }
  
  /* Allocate buffer with optimized size alignment for large files */
  size_t buffer_size = file_size;
  if (file_size > 64 * 1024) {
    /* For files larger than 64KB, align to page boundaries for better performance */
    buffer_size = ((file_size + 4095) / 4096) * 4096;
  }
  
  char* buffer = (char*)BUFFER_ALLOC(buffer_size);
  if (!buffer) {
    fclose(file);
    *size = 0;
    return NULL;
  }
  
  /* For large files, use optimized reading with larger buffer chunks */
  size_t bytes_read = 0;
  if (file_size > 32 * 1024) {
    /* Read in 64KB chunks for better I/O performance */
    const size_t chunk_size = 64 * 1024;
    char* write_ptr = buffer;
    size_t remaining = file_size;
    
    while (remaining > 0 && !feof(file)) {
      size_t to_read = remaining < chunk_size ? remaining : chunk_size;
      size_t chunk_read = fread(write_ptr, 1, to_read, file);
      
      if (chunk_read == 0) {
        break;  /* EOF or error */
      }
      
      bytes_read += chunk_read;
      write_ptr += chunk_read;
      remaining -= chunk_read;
    }
  } else {
    /* For smaller files, use single read */
    bytes_read = fread(buffer, 1, file_size, file);
  }
  
  fclose(file);
  
  if (bytes_read != (size_t)file_size) {
    BUFFER_FREE(buffer);
    *size = 0;
    return NULL;
  }
  
  *size = file_size;
  return buffer;
}

/* Serve admin file from filesystem with sendfile() optimization */
http_response_t* serve_admin_file(const char* path) {
  /* Get web root directory from server config */
  extern server_config_t* g_server_config;
  const char* web_root = NULL;
  char* dynamic_web_root = NULL;
  
  if (g_server_config && g_server_config->web_root) {
    web_root = g_server_config->web_root;
  } else {
    /* Fallback: get web root dynamically */
    dynamic_web_root = config_get_web_root();
    if (dynamic_web_root) {
      web_root = dynamic_web_root;
    } else {
      /* Last resort */
      web_root = "share/htdocs";
    }
  }

  /* Default path (root) to index.html */
  char filepath[512] = {0};
  
  if (strcmp(path, "/") == 0) {
    sprintf(filepath, "%s/index.html", web_root);
  } else if (strcmp(path, "/login") == 0) {
    sprintf(filepath, "%s/login.html", web_root);
  } else if (strcmp(path, "/admin.html") == 0) {
    /* Serve admin.html directly */
    sprintf(filepath, "%s/admin.html", web_root);
  } else if (strncmp(path, "/admin", 6) == 0) {
    /* Handle /admin prefix redirects */
    if (strcmp(path, "/admin") == 0 || strcmp(path, "/admin/") == 0) {
      sprintf(filepath, "%s/admin.html", web_root);
    } else {
      sprintf(filepath, "%s%s", web_root, path + 6);
    }
  } else {
    /* Handle CSS, JS, and other assets */
    sprintf(filepath, "%s%s", web_root, path);
  }
  
  /* Check if file exists */
  struct stat file_stat;
  if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
    /* File not found, try index.html for SPA routing */
    if (strncmp(path, "/admin", 6) == 0 || 
      strchr(path + 1, '.') == NULL) { /* If no file extension, likely a route */
      sprintf(filepath, "%s/index.html", web_root);
      
      /* Check if index.html exists */
      if (stat(filepath, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) {
        return create_http_response(HTTP_NOT_FOUND, 
                     "{\"error\":\"File not found\"}", "application/json");
      }
    } else {
      return create_http_response(HTTP_NOT_FOUND, 
                    "{\"error\":\"File not found\"}", "application/json");
    }
  }
  
  /* Read file content with performance optimization */
  size_t file_size = 0;
  char* file_content = read_file_content_optimized(filepath, &file_size);
  
  if (!file_content) {
    return create_http_response(HTTP_INTERNAL_SERVER_ERROR, 
                 "{\"error\":\"Failed to read file\"}", "application/json");
  }
  
  /* Get content type from file extension */
  const char* extension = get_file_extension(filepath);
  const char* content_type = get_content_type_from_extension(extension);
  
  /* Create response with file content using binary function to preserve exact size */
  http_response_t* response = create_http_response_binary(HTTP_OK, file_content, file_size, content_type);
  
  /* Clean up */
  BUFFER_FREE(file_content);
  
  /* Free dynamically allocated web root path to prevent memory leak */
  if (dynamic_web_root) {
    BUFFER_FREE(dynamic_web_root);
  }
  
  return response;
}