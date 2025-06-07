#include "manager.h"
#include <glib.h>
#include <stdio.h>
#include <stdarg.h>

struct BrowserTask {
    GtkWidget *row;
    GtkWidget *bar;
    gchar *name;
    gboolean cancelable;
    gpointer data;
};

static GList *tasks = NULL;
static GtkWidget *window = NULL;
static GtkWidget *list = NULL;
static gchar *log_path = NULL;

void manager_init(void) {
    gchar *cache_dir = g_build_filename(g_get_user_cache_dir(), "openb", NULL);
    g_mkdir_with_parents(cache_dir, 0755);
    log_path = g_build_filename(cache_dir, "error.log", NULL);
    g_free(cache_dir);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Task Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
    list = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(window), list);
}

void manager_shutdown(void) {
    g_list_free_full(tasks, (GDestroyNotify)g_free);
    tasks = NULL;
    g_free(log_path);
}

BrowserTask *manager_add_task(const gchar *name, gboolean cancelable, gpointer data) {
    BrowserTask *t = g_new0(BrowserTask, 1);
    t->name = g_strdup(name);
    t->cancelable = cancelable;
    t->data = data;
    t->row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *label = gtk_label_new(name);
    t->bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(t->row), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(t->row), t->bar, FALSE, FALSE, 0);
    gtk_list_box_insert(GTK_LIST_BOX(list), t->row, -1);
    gtk_widget_show_all(t->row);
    tasks = g_list_prepend(tasks, t);
    return t;
}

void manager_update_task(BrowserTask *t, gdouble progress) {
    if (t && t->bar)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(t->bar), progress);
}

void manager_remove_task(BrowserTask *t) {
    if (!t) return;
    gtk_container_remove(GTK_CONTAINER(list), t->row);
    tasks = g_list_remove(tasks, t);
    g_free(t->name);
    g_free(t);
}

static void manager_log_error(const gchar *msg) {
    if (!log_path) return;
    FILE *f = fopen(log_path, "a");
    if (!f) return;
    GDateTime *dt = g_date_time_new_now_local();
    gchar *ts = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
    fprintf(f, "[%s] %s\n", ts, msg);
    g_free(ts);
    g_date_time_unref(dt);
    fclose(f);
}

void manager_show_error(GtkWindow *parent, const gchar *fmt, ...) {
    va_list ap;
    gchar *msg;
    va_start(ap, fmt);
    msg = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    manager_log_error(msg);
    GtkWidget *d = gtk_message_dialog_new(parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    g_free(msg);
}

GtkWidget *manager_get_window(void) {
    return window;
}

