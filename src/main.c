#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>

static WebKitWebView *web_view;
static GtkEntry *url_entry;
static GtkWidget *progress_bar;
static GtkWidget *status_label;
static const char *program_path;
static GtkWidget *main_window;
static gboolean inspector_visible = FALSE;

static gchar *home_file_uri = NULL;

static void load_home_page(void) {
    if (!home_file_uri) {
        gchar *cwd = g_get_current_dir();
        if (!cwd)
            return;
        gchar *path = g_build_filename(cwd, "data", "home.html", NULL);
        g_free(cwd);
        if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_free(path);
            path = g_build_filename("/usr/local/share/openb", "home.html", NULL);
        }
        if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) {
            GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "Home page not found:\n%s",
                path ? path : "data/home.html");
            gtk_dialog_run(GTK_DIALOG(d));
            gtk_widget_destroy(d);
            g_free(path);
            return;
        }
        home_file_uri = g_strdup_printf("file://%s", path);
        g_free(path);
    }
    if (home_file_uri)
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
        gtk_label_set_text(GTK_LABEL(status_label), "Loading...");
    } else if (event == WEBKIT_LOAD_COMMITTED) {
        const gchar *uri = webkit_web_view_get_uri(view);
        if (home_file_uri && g_strcmp0(uri, home_file_uri) == 0)
            gtk_entry_set_text(url_entry, "");
        else
            gtk_entry_set_text(url_entry, uri ? uri : "");
    } else if (event == WEBKIT_LOAD_FINISHED) {
        gtk_widget_hide(progress_bar);
        gtk_label_set_text(GTK_LABEL(status_label), "Done");
    }
    return FALSE;
}

static void progress_changed(WebKitWebView *view, GParamSpec *pspec, gpointer data) {
    double progress = webkit_web_view_get_estimated_load_progress(view);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);
    gchar *pct = g_strdup_printf("%d%%", (int)(progress * 100));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), pct);
    gtk_label_set_text(GTK_LABEL(status_label), pct);
    g_free(pct);
}

static gboolean load_failed(WebKitWebView *view,
                            WebKitLoadEvent event,
                            const gchar *uri,
                            GError *error,
                            gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_NONE,
        "Failed to load %s:\n%s",
        uri,
        error->message);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Retry", GTK_RESPONSE_YES);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Close", GTK_RESPONSE_CLOSE);
    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (resp == GTK_RESPONSE_YES)
        webkit_web_view_reload(view);
    return TRUE;
}

static void mouse_target_changed(WebKitWebView *view,
                                 WebKitHitTestResult *hit,
                                 guint modifiers,
                                 gpointer data) {
    const gchar *link = webkit_hit_test_result_get_link_uri(hit);
    if (link)
        gtk_label_set_text(GTK_LABEL(status_label), link);
    else
        gtk_label_set_text(GTK_LABEL(status_label), "");
}

static void on_new_window(GtkWidget *widget, gpointer data) {
    if (program_path)
        g_spawn_command_line_async(program_path, NULL);
}

static void toggle_inspector(GtkWidget *widget, gpointer data) {
    WebKitWebInspector *inspector = webkit_web_view_get_inspector(web_view);
    if (webkit_web_inspector_get_web_view(inspector)) {
        webkit_web_inspector_close(inspector);
        inspector_visible = FALSE;
    } else {
        webkit_web_inspector_show(inspector);
        inspector_visible = TRUE;
    }
}

static gboolean perform_update(void) {
    if (!g_find_program_in_path("git") || !g_find_program_in_path("make")) {
        GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "git and make are required for updating.");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return FALSE;
    }

    if (!g_file_test(".git", G_FILE_TEST_IS_DIR)) {
        GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "No git repository found. Update cannot continue.");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return FALSE;
    }

    GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        "Updating OpenB...\nThis may take a moment.");
    gtk_widget_show(info);
    while (gtk_events_pending())
        gtk_main_iteration();

    const gchar *cmd[] = {"/bin/sh", "-c", "git pull --rebase && make", NULL};
    gint status = 0;
    GError *error = NULL;
    g_spawn_sync(NULL, (gchar **)cmd, NULL, G_SPAWN_SEARCH_PATH,
                 NULL, NULL, NULL, NULL, &status, &error);

    gtk_widget_destroy(info);

    if (error || status != 0) {
        GtkWidget *fail = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Update failed: %s", error ? error->message : "unknown error");
        gtk_dialog_run(GTK_DIALOG(fail));
        gtk_widget_destroy(fail);
        if (error)
            g_error_free(error);
        return FALSE;
    }

    GtkWidget *done = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_CLOSE,
        "Update complete. Restart OpenB to use the new version.");
    gtk_dialog_run(GTK_DIALOG(done));
    gtk_widget_destroy(done);
    return TRUE;
}

static void check_for_updates(GtkWidget *widget, gpointer data) {
    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Check for updates and rebuild?");
    gint res = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (res == GTK_RESPONSE_YES)
        perform_update();
}

static void show_about(GtkWidget *widget, gpointer data) {
    GtkAboutDialog *dialog = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
    gtk_about_dialog_set_program_name(dialog, "OpenB");
    gtk_about_dialog_set_version(dialog, "1.0");
    gtk_about_dialog_set_comments(dialog,
        "Lightweight WebKit browser built with GTK 3.");
    gtk_about_dialog_set_website(dialog,
        "https://github.com/Kgkkjjj/WebBrowser");
    const gchar *authors[] = { "OpenB contributors", NULL };
    gtk_about_dialog_set_authors(dialog, authors);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(data));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

int main(int argc, char *argv[]) {
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("Failed to initialize GTK.\n");
        return 1;
    }

    program_path = argv[0];

    gchar *data_dir = g_build_filename(g_get_user_data_dir(), "openb", NULL);
    gchar *cache_dir = g_build_filename(g_get_user_cache_dir(), "openb", NULL);
    WebKitWebsiteDataManager *manager = webkit_website_data_manager_new(
        "base-data-directory", data_dir,
        "base-cache-directory", cache_dir,
        NULL);

    WebKitWebContext *context = webkit_web_context_new_with_website_data_manager(manager);
    webkit_web_context_set_cache_model(context, WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    main_window = window;
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(window), "OpenB");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_toolbar_new();
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);

    GtkToolItem *back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
    GtkToolItem *forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    GtkToolItem *reload = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
    GtkToolItem *stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
    GtkToolItem *home = gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
    GtkToolItem *new_window = gtk_tool_button_new_from_stock(GTK_STOCK_NEW);
    GtkToolItem *update_btn = gtk_tool_button_new(NULL, "Update");
    GtkWidget *update_icon = gtk_image_new_from_icon_name("system-software-update", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(update_btn), update_icon);
    gtk_widget_show(update_icon);
    GtkToolItem *inspector_btn = gtk_tool_button_new(NULL, "DevTools");
    GtkWidget *inspector_icon = gtk_image_new_from_icon_name("applications-development", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(inspector_btn), inspector_icon);
    gtk_widget_show(inspector_icon);
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
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_window, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), update_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), inspector_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), entry_item, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), about, -1);

    gtk_widget_add_accelerator(GTK_WIDGET(reload), "clicked", accel, GDK_KEY_F5, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(home), "clicked", accel, GDK_KEY_F6, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(new_window), "clicked", accel, GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(inspector_btn), "clicked", accel, GDK_KEY_F12, 0, GTK_ACCEL_VISIBLE);
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(context));
    webkit_settings_set_enable_developer_extras(webkit_web_view_get_settings(web_view), TRUE);
    g_signal_connect(back, "clicked", G_CALLBACK(navigate_back), NULL);
    g_signal_connect(forward, "clicked", G_CALLBACK(navigate_forward), NULL);
    g_signal_connect(reload, "clicked", G_CALLBACK(reload_page), NULL);
    g_signal_connect(stop, "clicked", G_CALLBACK(stop_loading), NULL);
    g_signal_connect(home, "clicked", G_CALLBACK(navigate_home), NULL);
    g_signal_connect(new_window, "clicked", G_CALLBACK(on_new_window), NULL);
    g_signal_connect(update_btn, "clicked", G_CALLBACK(check_for_updates), NULL);
    g_signal_connect(inspector_btn, "clicked", G_CALLBACK(toggle_inspector), NULL);
    g_signal_connect(about, "clicked", G_CALLBACK(show_about), window);
    g_signal_connect(url_entry, "activate", G_CALLBACK(on_url_activate), NULL);
    g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), NULL);
    g_signal_connect(web_view, "notify::estimated-load-progress", G_CALLBACK(progress_changed), NULL);
    g_signal_connect(web_view, "load-failed", G_CALLBACK(load_failed), NULL);
    g_signal_connect(web_view, "mouse-target-changed", G_CALLBACK(mouse_target_changed), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_widget_set_no_show_all(progress_bar, TRUE);

    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(web_view), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);
    status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    load_home_page();

    gtk_widget_show_all(window);
    gtk_main();

    g_free(data_dir);
    g_free(cache_dir);
    g_object_unref(manager);
    g_object_unref(context);
    return 0;
}
