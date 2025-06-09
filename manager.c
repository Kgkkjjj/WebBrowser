#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

static const char *log_path = "openb.log";

void manager_set_log_file(const char *path)
{
    if (path && *path)
        log_path = path;
}

static void rotate_log(void)
{
    struct stat st;
    if (stat(log_path, &st) == 0 && st.st_size > 1024 * 1024) {
        char bak[256];
        snprintf(bak, sizeof(bak), "%s.1", log_path);
        rename(log_path, bak);
    }
}

static void log_to_file(const char *level, const char *msg)
{
    rotate_log();
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    time_t t = time(NULL);
    char *tstr = ctime(&t);
    tstr[strcspn(tstr, "\n")] = '\0';
    fprintf(f, "%s [%s] %s\n", tstr, level, msg);
    fclose(f);
}

void manager_log_error(const char *msg)
{
    fprintf(stderr, "OpenB error: %s\n", msg);
    log_to_file("ERROR", msg);
}

void manager_log_warning(const char *msg)
{
    fprintf(stderr, "OpenB warning: %s\n", msg);
    log_to_file("WARN", msg);
}

void manager_log_info(const char *msg)
{
    log_to_file("INFO", msg);
}
