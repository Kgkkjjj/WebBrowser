#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static WebKitWebView *web_view;

static void navigate_home(GtkWidget *widget, gpointer data) {
    webkit_web_view_load_uri(web_view, "about:home");
}

static void navigate_back(GtkWidget *widget, gpointer data) {
    if (webkit_web_view_can_go_back(web_view))
        webkit_web_view_go_back(web_view);
}

static void navigate_forward(GtkWidget *widget, gpointer data) {
    if (webkit_web_view_can_go_forward(web_view))
        webkit_web_view_go_forward(web_view);
}

static void reload_page(GtkWidget *widget, gpointer data) {
    webkit_web_view_reload(web_view);
}

static void on_search_activate(GtkEntry *entry, gpointer user_data) {
    const gchar *query = gtk_entry_get_text(entry);
    if (query && *query) {
        gchar *uri = g_strdup_printf("https://duckduckgo.com/?q=%s", query);
        webkit_web_view_load_uri(web_view, uri);
        g_free(uri);
    }
}

static gboolean load_changed(WebKitWebView *view, WebKitLoadEvent event, gpointer data) {
    if (event == WEBKIT_LOAD_FINISHED) {
        const gchar *uri = webkit_web_view_get_uri(view);
        if (g_strcmp0(uri, "about:home") == 0) {
            const gchar *home_html =
                "<html><body style='font-family:sans-serif; text-align:center;'>"
                "<h1>Welcome</h1>"
                "<form action='https://duckduckgo.com/' method='GET'>"
                "<input type='text' name='q' style='width:60%; padding:5px;'/>"
                "<input type='submit' value='Search'/>"
                "</form>"
                "<p><a href='file:///downloads/'>Downloads</a></p>"
                "</body></html>";
            webkit_web_view_load_html(view, home_html, "about:home");
        }
    }
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Simple Browser");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_toolbar_new();

    GtkToolItem *back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    GtkToolItem *forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    GtkToolItem *reload = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
    GtkToolItem *home = gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
    GtkToolItem *separator = gtk_separator_tool_item_new();
    GtkWidget *search_entry = gtk_entry_new();
    GtkToolItem *search_item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(search_item), search_entry);
    gtk_tool_item_set_expand(search_item, TRUE);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), home, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), search_item, -1);

    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_signal_connect(back, "clicked", G_CALLBACK(navigate_back), NULL);
    g_signal_connect(forward, "clicked", G_CALLBACK(navigate_forward), NULL);
    g_signal_connect(reload, "clicked", G_CALLBACK(reload_page), NULL);
    g_signal_connect(home, "clicked", G_CALLBACK(navigate_home), NULL);
    g_signal_connect(search_entry, "activate", G_CALLBACK(on_search_activate), NULL);
    g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(web_view), TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    webkit_web_view_load_uri(web_view, "about:home");

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
