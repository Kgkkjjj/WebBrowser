#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

static WebKitWebView *web_view;
static GtkEntry *url_entry;
static GtkWidget *progress_bar;

static gchar *home_file_uri = NULL;

static void load_home_page(void) {
    if (!home_file_uri) {
        gchar *cwd = g_get_current_dir();
        gchar *path = g_build_filename(cwd, "data", "home.html", NULL);
        home_file_uri = g_strdup_printf("file://%s", path);
        g_free(path);
        g_free(cwd);
    }
    webkit_web_view_load_uri(web_view, home_file_uri);
}

static void navigate_home(GtkWidget *widget, gpointer data) {
    load_home_page();
}

static void stop_loading(GtkWidget *widget, gpointer data) {
    webkit_web_view_stop_loading(web_view);
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

static void on_url_activate(GtkEntry *entry, gpointer user_data) {
    const gchar *text = gtk_entry_get_text(entry);
    if (!text || !*text)
        return;

    gchar *uri = NULL;
    if (g_str_has_prefix(text, "http://") || g_str_has_prefix(text, "https://")) {
        uri = g_strdup(text);
    } else {
        gchar *escaped = g_uri_escape_string(text, NULL, TRUE);
        uri = g_strdup_printf("https://duckduckgo.com/?q=%s", escaped);
        g_free(escaped);
    }

    webkit_web_view_load_uri(web_view, uri);
    g_free(uri);
}

static gboolean load_changed(WebKitWebView *view, WebKitLoadEvent event, gpointer data) {
    if (event == WEBKIT_LOAD_STARTED) {
        gtk_widget_show(progress_bar);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    } else if (event == WEBKIT_LOAD_COMMITTED) {
        const gchar *uri = webkit_web_view_get_uri(view);
        if (home_file_uri && g_strcmp0(uri, home_file_uri) == 0)
            gtk_entry_set_text(url_entry, "");
        else
            gtk_entry_set_text(url_entry, uri ? uri : "");
    } else if (event == WEBKIT_LOAD_FINISHED) {
        gtk_widget_hide(progress_bar);
    }
    return FALSE;
}

static void progress_changed(WebKitWebView *view, GParamSpec *pspec, gpointer data) {
    double progress = webkit_web_view_get_estimated_load_progress(view);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);
}

static void show_about(GtkWidget *widget, gpointer data) {
    GtkWindow *parent = GTK_WINDOW(data);
    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Simple Browser\nBuilt with GTK 3 and WebKit2GTK"
    );
    gtk_window_set_title(GTK_WINDOW(dialog), "About Simple Browser");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
    GtkToolItem *stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
    GtkToolItem *home = gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
    GtkToolItem *about = gtk_tool_button_new_from_stock(GTK_STOCK_ABOUT);
    GtkToolItem *separator = gtk_separator_tool_item_new();
    GtkWidget *entry_widget = gtk_entry_new();
    url_entry = GTK_ENTRY(entry_widget);
    GtkToolItem *entry_item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(entry_item), entry_widget);
    gtk_tool_item_set_expand(entry_item, TRUE);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), home, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), entry_item, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), about, -1);

    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_signal_connect(back, "clicked", G_CALLBACK(navigate_back), NULL);
    g_signal_connect(forward, "clicked", G_CALLBACK(navigate_forward), NULL);
    g_signal_connect(reload, "clicked", G_CALLBACK(reload_page), NULL);
    g_signal_connect(stop, "clicked", G_CALLBACK(stop_loading), NULL);
    g_signal_connect(home, "clicked", G_CALLBACK(navigate_home), NULL);
    g_signal_connect(about, "clicked", G_CALLBACK(show_about), window);
    g_signal_connect(url_entry, "activate", G_CALLBACK(on_url_activate), NULL);
    g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), NULL);
    g_signal_connect(web_view, "notify::estimated-load-progress", G_CALLBACK(progress_changed), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    progress_bar = gtk_progress_bar_new();
    gtk_widget_set_no_show_all(progress_bar, TRUE);

    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(web_view), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    load_home_page();

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
