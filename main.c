#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

/* manager.c API */
void manager_log_error(const char *msg);

typedef struct {
    GtkWidget *window;
    GtkWidget *url_entry;
} App;

static gboolean launch_netsurf(const char *target)
{
    if (!target || !*target)
        return FALSE;

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

