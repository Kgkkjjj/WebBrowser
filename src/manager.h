#ifndef OPENB_MANAGER_H
#define OPENB_MANAGER_H

#include <gtk/gtk.h>

typedef struct BrowserTask BrowserTask;
typedef void (*ManagerErrorFunc)(const gchar *msg, gpointer data);

void manager_init(void);
void manager_shutdown(void);
BrowserTask *manager_add_task(const gchar *name, gboolean cancelable, gpointer data);
void manager_update_task(BrowserTask *task, gdouble progress);
void manager_remove_task(BrowserTask *task);
void manager_show_error(GtkWindow *parent, const gchar *fmt, ...);
GtkWidget *manager_get_window(void);
void manager_set_error_callback(ManagerErrorFunc cb, gpointer data);
void manager_set_log_max_size(gsize bytes);
void manager_clear_log(void);
gchar *manager_get_last_error(void);
void manager_log_warning(const gchar *fmt, ...);
void manager_log_info(const gchar *fmt, ...);
void manager_show_warning(GtkWindow *parent, const gchar *fmt, ...);
void manager_show_critical(GtkWindow *parent, const gchar *fmt, ...);
void manager_open_log(GtkWindow *parent);
void manager_show_error_from_gerror(GtkWindow *parent, GError *error);

#endif /* OPENB_MANAGER_H */
