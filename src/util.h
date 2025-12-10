#ifndef LKJ_UTIL_H
#define LKJ_UTIL_H

#include <stddef.h>
#include <stdbool.h>

size_t lkj_strlcpy(char *dst, const char *src, size_t size);
bool lkj_url_decode(const char *src, char *dest, size_t dest_size);
const char *lkj_mime_type(const char *path);

#endif
