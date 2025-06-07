#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

/* manager.c functions */
void manager_log_error(const char *msg);

static void load_failed(WebKitWebView *view,
                        WebKitLoadEvent event,
                        const gchar *uri,
                        GError *error,
                        gpointer user_data)
{
    (void)view; (void)event; (void)uri; (void)user_data;
    manager_log_error(error->message);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_signal_connect(view, "load-failed", G_CALLBACK(load_failed), NULL);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

    const char *uri = argc > 1 ? argv[1] : "https://example.com";
    webkit_web_view_load_uri(view, uri);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
