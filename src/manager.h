#ifndef OPENB_MANAGER_H
#define OPENB_MANAGER_H

#include <gtk/gtk.h>

typedef struct BrowserTask BrowserTask;

void manager_init(void);
void manager_shutdown(void);
BrowserTask *manager_add_task(const gchar *name, gboolean cancelable, gpointer data);
void manager_update_task(BrowserTask *task, gdouble progress);
void manager_remove_task(BrowserTask *task);
void manager_show_error(GtkWindow *parent, const gchar *fmt, ...);
GtkWidget *manager_get_window(void);

#endif /* OPENB_MANAGER_H */
