#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "util.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct client_ctx {
    int fd;
    struct lkj_server_config config;
};

static void *client_thread(void *arg);
static int create_listener(const char *host, int port);
static int send_response(int fd, const char *status, const char *content_type, const char *body, size_t body_len);
static int send_file_response(int fd, const char *path);
static int handle_request(int fd, const struct lkj_server_config *config, const char *req_buf, size_t req_len);
static const char *skip_spaces(const char *p);
static int read_request(int fd, char *buffer, size_t buffer_size, size_t *out_len);
static int handle_api_request(int fd, const struct lkj_server_config *config, const char *method, const char *path, const char *query, const char *body, size_t body_len);
static int handle_list(const struct lkj_server_config *config, int fd, const char *query);
static int handle_read(const struct lkj_server_config *config, int fd, const char *query);
static int handle_write(const struct lkj_server_config *config, int fd, const char *query, const char *body, size_t body_len);
static int handle_exec(int fd, const char *body, size_t body_len);
static int sanitize_and_join(const char *root, const char *relative, char *out, size_t out_size);
static const char *query_param(const char *query, const char *key);
static const char *case_insensitive_search(const char *haystack, const char *needle);

int lkj_server_run(const struct lkj_server_config *config) {
    int listener = create_listener(config->host, config->port);
    if (listener < 0) {
        return 1;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        struct client_ctx *ctx = calloc(1, sizeof(struct client_ctx));
        if (!ctx) {
            perror("calloc");
            close(client_fd);
            continue;
        }
        ctx->fd = client_fd;
        ctx->config = *config;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(ctx);
            continue;
        }
        pthread_detach(tid);
    }
}

static void *client_thread(void *arg) {
    struct client_ctx *ctx = (struct client_ctx *)arg;
    char buffer[LKJ_BUFFER_SIZE];
    size_t req_len = 0;
    if (read_request(ctx->fd, buffer, sizeof(buffer), &req_len) == 0) {
        handle_request(ctx->fd, &ctx->config, buffer, req_len);
    }
    close(ctx->fd);
    free(ctx);
    return NULL;
}

static int create_listener(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static int read_request(int fd, char *buffer, size_t buffer_size, size_t *out_len) {
    size_t total = 0;
    while (total < buffer_size - 1) {
        ssize_t n = recv(fd, buffer + total, buffer_size - 1 - total, 0);
        if (n < 0) {
            perror("recv");
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n")) {
            break;
        }
    }

    const char *cl_header = case_insensitive_search(buffer, "Content-Length:");
    size_t content_length = 0;
    if (cl_header) {
        cl_header += strlen("Content-Length:");
        cl_header = skip_spaces(cl_header);
        content_length = (size_t)strtoul(cl_header, NULL, 10);
    }

    const char *body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) {
        return -1;
    }
    body_start += 4;
    size_t header_len = (size_t)(body_start - buffer);
    size_t current_body = total - header_len;

    while (current_body < content_length && total < buffer_size - 1) {
        ssize_t n = recv(fd, buffer + total, buffer_size - 1 - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
        current_body += (size_t)n;
    }

    buffer[total] = '\0';
    *out_len = total;
    return 0;
}

static int handle_request(int fd, const struct lkj_server_config *config, const char *req_buf, size_t req_len) {
    (void)req_len;
    char method[8] = {0};
    char path[LKJ_MAX_PATH] = {0};
    char protocol[16] = {0};

    const char *line_end = strstr(req_buf, "\r\n");
    if (!line_end) {
        return send_response(fd, "400 Bad Request", "text/plain", "Bad Request", strlen("Bad Request"));
    }

    if (sscanf(req_buf, "%7s %4095s %15s", method, path, protocol) != 3) {
        return send_response(fd, "400 Bad Request", "text/plain", "Bad Request", strlen("Bad Request"));
    }

    char *query = strchr(path, '?');
    if (query) {
        *query++ = '\0';
    }

    const char *body = strstr(req_buf, "\r\n\r\n");
    size_t body_len = 0;
    if (body) {
        body += 4;
        body_len = (size_t)((req_buf + req_len) - body);
    }

    if (strcmp(path, "/") == 0 && strcmp(method, "GET") == 0) {
        char index_path[LKJ_MAX_PATH];
        snprintf(index_path, sizeof(index_path), "%s/index.html", config->static_root);
        return send_file_response(fd, index_path);
    }

    if (strncmp(path, "/assets/", 8) == 0 && strcmp(method, "GET") == 0) {
        char asset_path[LKJ_MAX_PATH];
        snprintf(asset_path, sizeof(asset_path), "%s/%s", config->static_root, path + 8);
        return send_file_response(fd, asset_path);
    }

    if (strncmp(path, "/api/", 5) == 0) {
        return handle_api_request(fd, config, method, path, query, body ? body : "", body_len);
    }

    return send_response(fd, "404 Not Found", "text/plain", "Not Found", strlen("Not Found"));
}

static int send_response(int fd, const char *status, const char *content_type, const char *body, size_t body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              status, content_type, body_len);
    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return -1;
    }
    if (send(fd, header, (size_t)header_len, 0) < 0) {
        return -1;
    }
    if (body_len > 0 && send(fd, body, body_len, 0) < 0) {
        return -1;
    }
    return 0;
}

static int send_file_response(int fd, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        const char *msg = "Not Found";
        return send_response(fd, "404 Not Found", "text/plain", msg, strlen(msg));
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size > LKJ_BUFFER_SIZE) {
        fclose(f);
        const char *msg = "File Too Large";
        return send_response(fd, "413 Payload Too Large", "text/plain", msg, strlen(msg));
    }

    char *content = malloc((size_t)size);
    if (!content) {
        fclose(f);
        return send_response(fd, "500 Internal Server Error", "text/plain", "OOM", 3);
    }

    size_t read_bytes = fread(content, 1, (size_t)size, f);
    if (read_bytes != (size_t)size) {
        fclose(f);
        free(content);
        const char *msg = "Failed to read file";
        return send_response(fd, "500 Internal Server Error", "text/plain", msg, strlen(msg));
    }
    fclose(f);

    const char *mime = lkj_mime_type(path);
    int res = send_response(fd, "200 OK", mime, content, (size_t)size);
    free(content);
    return res;
}

static int handle_api_request(int fd, const struct lkj_server_config *config, const char *method, const char *path, const char *query, const char *body, size_t body_len) {
    if (strcmp(path, "/api/files") == 0 && strcmp(method, "GET") == 0) {
        return handle_list(config, fd, query);
    }
    if (strcmp(path, "/api/file") == 0 && strcmp(method, "GET") == 0) {
        return handle_read(config, fd, query);
    }
    if (strcmp(path, "/api/file") == 0 && strcmp(method, "POST") == 0) {
        return handle_write(config, fd, query, body, body_len);
    }
    if (strcmp(path, "/api/exec") == 0 && strcmp(method, "POST") == 0) {
        return handle_exec(fd, body, body_len);
    }
    const char *msg = "Unsupported";
    return send_response(fd, "405 Method Not Allowed", "text/plain", msg, strlen(msg));
}

static const char *query_param(const char *query, const char *key) {
    if (!query || !key) return NULL;
    size_t key_len = strlen(key);
    const char *p = query;
    while (p && *p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            return p + key_len + 1;
        }
        p = strchr(p, '&');
        if (p) ++p;
    }
    return NULL;
}

static int sanitize_and_join(const char *root, const char *relative, char *out, size_t out_size) {
    if (!relative || strstr(relative, "..")) {
        return -1;
    }
    while (*relative == '/') {
        relative++;
    }
    if (snprintf(out, out_size, "%s/%s", root, relative) >= (int)out_size) {
        return -1;
    }
    return 0;
}

static int handle_list(const struct lkj_server_config *config, int fd, const char *query) {
    const char *raw_path = query_param(query, "path");
    char decoded[LKJ_MAX_PATH];
    if (!raw_path || !lkj_url_decode(raw_path, decoded, sizeof(decoded))) {
        const char *msg = "Missing path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    char abs_path[LKJ_MAX_PATH];
    if (sanitize_and_join(config->workspace_root, decoded, abs_path, sizeof(abs_path)) != 0) {
        const char *msg = "Invalid path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    DIR *dir = opendir(abs_path);
    if (!dir) {
        const char *msg = "Cannot open directory";
        return send_response(fd, "404 Not Found", "text/plain", msg, strlen(msg));
    }

    char response[LKJ_BUFFER_SIZE];
    size_t offset = 0;
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "{\"path\":\"%s\",\"entries\":[", decoded);

    struct dirent *entry;
    int first = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!first) {
            response[offset++] = ',';
        }
        first = 0;

        char item_path[LKJ_MAX_PATH];
        int name_len = snprintf(item_path, sizeof(item_path), "%s/%s", abs_path, entry->d_name);
        if (name_len < 0 || name_len >= (int)sizeof(item_path)) {
            continue;
        }
        struct stat st;
        stat(item_path, &st);
        const char *kind = S_ISDIR(st.st_mode) ? "dir" : "file";
        offset += (size_t)snprintf(response + offset, sizeof(response) - offset,
                                   "{\"name\":\"%s\",\"kind\":\"%s\",\"size\":%ld}",
                                   entry->d_name, kind, (long)st.st_size);
        if (offset >= sizeof(response)) {
            break;
        }
    }
    closedir(dir);

    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "]}");
    return send_response(fd, "200 OK", "application/json", response, offset);
}

static int handle_read(const struct lkj_server_config *config, int fd, const char *query) {
    const char *raw_path = query_param(query, "path");
    char decoded[LKJ_MAX_PATH];
    if (!raw_path || !lkj_url_decode(raw_path, decoded, sizeof(decoded))) {
        const char *msg = "Missing path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    char abs_path[LKJ_MAX_PATH];
    if (sanitize_and_join(config->workspace_root, decoded, abs_path, sizeof(abs_path)) != 0) {
        const char *msg = "Invalid path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    FILE *f = fopen(abs_path, "rb");
    if (!f) {
        const char *msg = "Cannot open file";
        return send_response(fd, "404 Not Found", "text/plain", msg, strlen(msg));
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || size >= (long)(LKJ_BUFFER_SIZE - 64)) {
        fclose(f);
        const char *msg = "File too large";
        return send_response(fd, "413 Payload Too Large", "text/plain", msg, strlen(msg));
    }

    char *content = malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return send_response(fd, "500 Internal Server Error", "text/plain", "OOM", 3);
    }
    size_t read_bytes = fread(content, 1, (size_t)size, f);
    content[size] = '\0';
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(content);
        const char *msg = "Failed to read file";
        return send_response(fd, "500 Internal Server Error", "text/plain", msg, strlen(msg));
    }

    char response[LKJ_BUFFER_SIZE];
    size_t offset = 0;
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "{\"path\":\"%s\",\"content\":\"", decoded);
    for (long i = 0; i < size && offset + 4 < sizeof(response); ++i) {
        unsigned char c = (unsigned char)content[i];
        switch (c) {
            case '"':
            case '\\':
                response[offset++] = '\\';
                response[offset++] = (char)c;
                break;
            case '\n':
                response[offset++] = '\\';
                response[offset++] = 'n';
                break;
            case '\r':
                response[offset++] = '\\';
                response[offset++] = 'r';
                break;
            case '\t':
                response[offset++] = '\\';
                response[offset++] = 't';
                break;
            default:
                if (isprint(c)) {
                    response[offset++] = (char)c;
                } else {
                    response[offset++] = ' ';
                }
        }
    }
    free(content);
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "\"}");

    return send_response(fd, "200 OK", "application/json", response, offset);
}

static int handle_write(const struct lkj_server_config *config, int fd, const char *query, const char *body, size_t body_len) {
    const char *raw_path = query_param(query, "path");
    char decoded[LKJ_MAX_PATH];
    if (!raw_path || !lkj_url_decode(raw_path, decoded, sizeof(decoded))) {
        const char *msg = "Missing path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    char abs_path[LKJ_MAX_PATH];
    if (sanitize_and_join(config->workspace_root, decoded, abs_path, sizeof(abs_path)) != 0) {
        const char *msg = "Invalid path";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    FILE *f = fopen(abs_path, "wb");
    if (!f) {
        const char *msg = "Cannot open file";
        return send_response(fd, "403 Forbidden", "text/plain", msg, strlen(msg));
    }

    size_t written = fwrite(body, 1, body_len, f);
    fclose(f);
    if (written != body_len) {
        const char *msg = "Failed to write file";
        return send_response(fd, "500 Internal Server Error", "text/plain", msg, strlen(msg));
    }

    const char *msg = "{\"status\":\"ok\"}";
    return send_response(fd, "200 OK", "application/json", msg, strlen(msg));
}

static int handle_exec(int fd, const char *body, size_t body_len) {
    if (body_len == 0) {
        const char *msg = "Missing command";
        return send_response(fd, "400 Bad Request", "text/plain", msg, strlen(msg));
    }

    char cmd[256];
    size_t len = body_len < sizeof(cmd) - 1 ? body_len : sizeof(cmd) - 1;
    memcpy(cmd, body, len);
    cmd[len] = '\0';

    FILE *p = popen(cmd, "r");
    if (!p) {
        const char *msg = "Cannot execute";
        return send_response(fd, "500 Internal Server Error", "text/plain", msg, strlen(msg));
    }

    char output[LKJ_BUFFER_SIZE];
    size_t n = fread(output, 1, sizeof(output) - 1, p);
    output[n] = '\0';
    pclose(p);

    char response[LKJ_BUFFER_SIZE];
    size_t offset = (size_t)snprintf(response, sizeof(response), "{\"cmd\":\"");
    for (size_t i = 0; i < len && offset + 4 < sizeof(response); ++i) {
        char c = cmd[i];
        if (c == '\\' || c == '"') {
            response[offset++] = '\\';
            response[offset++] = c;
        } else if (c == '\n') {
            response[offset++] = '\\';
            response[offset++] = 'n';
        } else {
            response[offset++] = c;
        }
    }
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "\",\"output\":\"");
    for (size_t i = 0; i < n && offset + 4 < sizeof(response); ++i) {
        unsigned char c = (unsigned char)output[i];
        switch (c) {
            case '"':
            case '\\':
                response[offset++] = '\\';
                response[offset++] = (char)c;
                break;
            case '\n':
                response[offset++] = '\\';
                response[offset++] = 'n';
                break;
            case '\r':
                response[offset++] = '\\';
                response[offset++] = 'r';
                break;
            default:
                if (isprint(c)) {
                    response[offset++] = (char)c;
                } else {
                    response[offset++] = ' ';
                }
        }
    }
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "\"}");

    return send_response(fd, "200 OK", "application/json", response, offset);
}

static const char *skip_spaces(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static const char *case_insensitive_search(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return NULL;
    }
    size_t needle_len = strlen(needle);
    for (const char *p = haystack; *p; ++p) {
        if (tolower((unsigned char)*p) == tolower((unsigned char)*needle)) {
            if (strncasecmp(p, needle, needle_len) == 0) {
                return p;
            }
        }
    }
    return NULL;
}
