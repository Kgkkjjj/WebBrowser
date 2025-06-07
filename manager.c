#include <stdio.h>
#include <time.h>

static void log_to_file(const char *msg)
{
    FILE *f = fopen("openb.log", "a");
    if (!f) return;
    time_t t = time(NULL);
    fprintf(f, "%s: %s\n", ctime(&t), msg);
    fclose(f);
}

void manager_log_error(const char *msg)
{
    fprintf(stderr, "OpenB error: %s\n", msg);
    log_to_file(msg);
}
