#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* manager.c API */
void manager_log_error(const char *msg);
void manager_log_warning(const char *msg);
void manager_log_info(const char *msg);

typedef struct {
    GtkWidget *window;
    GtkWidget *url_entry;
} App;

static gboolean launch_netsurf(const char *target)
{
    if (!target || !*target)
        target = ""; /* open blank */

    gchar *cmd = g_strdup_printf("netsurf-gtk '%s' &", target);
    int r = system(cmd);
    g_free(cmd);
    if (r != 0) {
        manager_log_error("Failed to launch NetSurf");
        return FALSE;
    }
    return TRUE;
}

static void on_open_url(GtkButton *button, gpointer data)
{
    (void)button;
    App *app = data;
    const char *url = gtk_entry_get_text(GTK_ENTRY(app->url_entry));
    if (!launch_netsurf(url))
        manager_log_error("Could not open URL");
}

static void on_open_file(GtkButton *button, gpointer data)
{
    (void)button;
    App *app = data;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Open File",
        GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        launch_netsurf(filename);
        g_free(filename);
    }
    gtk_widget_destroy(dlg);
}

static void on_open_bookmarks(GtkButton *button, gpointer data)
{
    (void)button; (void)data;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, ".netsurf", "Hotlist", NULL);
    launch_netsurf(path);
    g_free(path);
}

static void on_open_history(GtkButton *button, gpointer data)
{
    (void)button; (void)data;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, ".netsurf", "History", NULL);
    launch_netsurf(path);
    g_free(path);
}

static void on_about(GtkButton *button, gpointer data)
{
    (void)button; App *app = data;
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(app->window), 0,
        GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "OpenB (NetSurf Edition)\nA minimal browser using NetSurf engine.");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

/* New helper actions */
static void on_home(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    launch_netsurf("http://linuxksdteam.site");
}

static void on_search(GtkButton *b, gpointer data)
{
    (void)b;
    App *app = data;
    const char *q = gtk_entry_get_text(GTK_ENTRY(app->url_entry));
    if (!q || !*q) {
        manager_log_warning("Search query is empty");
        return;
    }
    gchar *url = g_strdup_printf("http://api.openb/search?q=%s", q);
    launch_netsurf(url);
    g_free(url);
}

static void on_prefs(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, ".netsurf", "Choices", NULL);
    launch_netsurf(path);
    g_free(path);
}

static void remove_file(const char *fname)
{
    if (g_remove(fname) != 0)
        manager_log_warning("Failed to remove file");
}

static void on_clear_cache(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, ".netsurf", "Cache", NULL);
    remove_file(path);
    g_free(path);
}

static void on_clear_cookies(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, ".netsurf", "Cookies", NULL);
    remove_file(path);
    g_free(path);
}

static void on_new_window(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    launch_netsurf("");
}

static void on_open_downloads(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    const char *home = g_get_home_dir();
    gchar *path = g_build_filename(home, "Downloads", NULL);
    launch_netsurf(path);
    g_free(path);
}

static void on_update(GtkButton *b, gpointer data)
{
    (void)b; (void)data;
    if (access(".git", F_OK) != 0) {
        manager_log_error("No git repository found");
        return;
    }
    int r = system("git pull --ff-only");
    if (r != 0)
        manager_log_error("Update failed");
}

static void on_help(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    launch_netsurf("https://netsurf-browser.org/documentation/");
}

static void on_terminal(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    system("x-terminal-emulator &");
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    App app;
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "OpenB");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 800, 600);
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app.window), vbox);

    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);

    GtkToolItem *url_item = gtk_tool_item_new();
    app.url_entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(url_item), app.url_entry);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), url_item, -1);

    GtkToolItem *open_btn = gtk_tool_button_new(NULL, "Open URL");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_url), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), open_btn, -1);

    GtkToolItem *file_btn = gtk_tool_button_new(NULL, "Open File");
    g_signal_connect(file_btn, "clicked", G_CALLBACK(on_open_file), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), file_btn, -1);

    GtkToolItem *bm_btn = gtk_tool_button_new(NULL, "Bookmarks");
    g_signal_connect(bm_btn, "clicked", G_CALLBACK(on_open_bookmarks), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), bm_btn, -1);

    GtkToolItem *hist_btn = gtk_tool_button_new(NULL, "History");
    g_signal_connect(hist_btn, "clicked", G_CALLBACK(on_open_history), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), hist_btn, -1);

    GtkToolItem *home_btn = gtk_tool_button_new(NULL, "Home");
    g_signal_connect(home_btn, "clicked", G_CALLBACK(on_home), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), home_btn, -1);

    GtkToolItem *search_btn = gtk_tool_button_new(NULL, "Search");
    g_signal_connect(search_btn, "clicked", G_CALLBACK(on_search), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), search_btn, -1);

    GtkToolItem *prefs_btn = gtk_tool_button_new(NULL, "Prefs");
    g_signal_connect(prefs_btn, "clicked", G_CALLBACK(on_prefs), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), prefs_btn, -1);

    GtkToolItem *cache_btn = gtk_tool_button_new(NULL, "Clear Cache");
    g_signal_connect(cache_btn, "clicked", G_CALLBACK(on_clear_cache), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), cache_btn, -1);

    GtkToolItem *cookie_btn = gtk_tool_button_new(NULL, "Clear Cookies");
    g_signal_connect(cookie_btn, "clicked", G_CALLBACK(on_clear_cookies), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), cookie_btn, -1);

    GtkToolItem *new_btn = gtk_tool_button_new(NULL, "New Window");
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_new_window), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_btn, -1);

    GtkToolItem *dl_btn = gtk_tool_button_new(NULL, "Downloads");
    g_signal_connect(dl_btn, "clicked", G_CALLBACK(on_open_downloads), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), dl_btn, -1);

    GtkToolItem *update_btn = gtk_tool_button_new(NULL, "Update");
    g_signal_connect(update_btn, "clicked", G_CALLBACK(on_update), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), update_btn, -1);

    GtkToolItem *help_btn = gtk_tool_button_new(NULL, "Help");
    g_signal_connect(help_btn, "clicked", G_CALLBACK(on_help), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), help_btn, -1);

    GtkToolItem *term_btn = gtk_tool_button_new(NULL, "Terminal");
    g_signal_connect(term_btn, "clicked", G_CALLBACK(on_terminal), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), term_btn, -1);

    GtkToolItem *about_btn = gtk_tool_button_new(NULL, "About");
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about), &app);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), about_btn, -1);

    GtkToolItem *quit_btn = gtk_tool_button_new(NULL, "Quit");
    g_signal_connect_swapped(quit_btn, "clicked", G_CALLBACK(gtk_widget_destroy), app.window);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), quit_btn, -1);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    GtkWidget *info = gtk_label_new("Enter a URL and click 'Open URL' to launch NetSurf.");
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 5);

    gtk_widget_show_all(app.window);

    if (argc > 1)
        launch_netsurf(argv[1]);

    gtk_main();
    return 0;
}
