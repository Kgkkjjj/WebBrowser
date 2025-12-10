#include "util.h"

#include <ctype.h>
#include <string.h>

size_t lkj_strlcpy(char *dst, const char *src, size_t size) {
    size_t i;
    for (i = 0; i + 1 < size && src[i]; ++i) {
        dst[i] = src[i];
    }
    if (size > 0) {
        dst[i] = '\0';
    }
    while (src[i]) {
        ++i;
    }
    return i;
}

bool lkj_url_decode(const char *src, char *dest, size_t dest_size) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di + 1 < dest_size; ++i) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            int high = isdigit((unsigned char)src[i + 1]) ? src[i + 1] - '0' : tolower((unsigned char)src[i + 1]) - 'a' + 10;
            int low = isdigit((unsigned char)src[i + 2]) ? src[i + 2] - '0' : tolower((unsigned char)src[i + 2]) - 'a' + 10;
            dest[di++] = (char)((high << 4) | low);
            i += 2;
        } else if (src[i] == '+') {
            dest[di++] = ' ';
        } else {
            dest[di++] = src[i];
        }
    }
    dest[di] = '\0';
    return di + 1 < dest_size;
}

const char *lkj_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "text/plain";
    }

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".png") == 0) return "image/png";

    return "application/octet-stream";
}
