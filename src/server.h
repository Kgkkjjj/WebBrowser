#ifndef LKJ_SERVER_H
#define LKJ_SERVER_H

#include <stddef.h>

#define LKJ_MAX_PATH 4096
#define LKJ_BUFFER_SIZE 65536

struct lkj_server_config {
    const char *host;
    int port;
    const char *workspace_root;
    const char *static_root;
};

int lkj_server_run(const struct lkj_server_config *config);

#endif
