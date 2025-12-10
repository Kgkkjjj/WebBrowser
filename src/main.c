#include "server.h"
#include "gui.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *get_env_or(const char *name, const char *fallback) {
    const char *value = getenv(name);
    return value ? value : fallback;
}

struct server_thread_args {
    struct lkj_server_config config;
};

static void *server_thread(void *arg) {
    struct server_thread_args *args = (struct server_thread_args *)arg;
    lkj_server_run(&args->config);
    free(args);
    return NULL;
}

int main(void) {
    const char *host = get_env_or("LKJ_HOST", "0.0.0.0");
    const char *port_str = get_env_or("LKJ_PORT", "8080");
    const char *workspace = get_env_or("LKJ_WORKSPACE", ".");
    const char *static_root = get_env_or("LKJ_STATIC", "assets");

    char *end = NULL;
    long port = strtol(port_str, &end, 10);
    if (!end || *end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", port_str);
        return 1;
    }

    struct lkj_server_config config = {
        .host = host,
        .port = (int)port,
        .workspace_root = workspace,
        .static_root = static_root,
    };

    printf("LKJ IDE listening on %s:%ld\n", host, port);
    printf("Workspace: %s\n", workspace);
    printf("Static assets: %s\n", static_root);

    if (lkj_gui_supported()) {
        struct server_thread_args *args = calloc(1, sizeof(*args));
        if (!args) {
            perror("calloc");
            return 1;
        }
        args->config = config;
        pthread_t tid;
        if (pthread_create(&tid, NULL, server_thread, args) != 0) {
            perror("pthread_create");
            free(args);
            return 1;
        }
        pthread_detach(tid);

        char url[128];
        const char *display_host = strcmp(host, "0.0.0.0") == 0 ? "127.0.0.1" : host;
        snprintf(url, sizeof(url), "http://%s:%ld", display_host, port);
        printf("Launching LKJ desktop window at %s\n", url);
        return lkj_launch_gui(url);
    }

    printf("GUI unavailable, running server only.\n");
    return lkj_server_run(&config);
}
