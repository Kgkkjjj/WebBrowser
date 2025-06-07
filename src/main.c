#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit2/webkit2.h>
#include <glib/gstdio.h>
#include <signal.h>
#include "manager.h"

static WebKitWebView *web_view;
static GtkEntry *url_entry;
static GtkWidget *progress_bar;
static GtkWidget *status_label;
static const char *program_path;
static GtkWidget *main_window;
static gboolean inspector_visible = FALSE;
static gdouble zoom_level = 1.0;
static gboolean js_enabled = TRUE;
static gboolean images_enabled = TRUE;
static gboolean dark_mode = FALSE;
static gboolean is_fullscreen = FALSE;

typedef struct DownloadRow {
    GtkWidget *row;
    GtkWidget *bar;
} DownloadRow;

static GtkWidget *downloads_window = NULL;
static GtkWidget *downloads_list = NULL;

static GSList *search_history = NULL;
static GSList *bookmarks = NULL;
static gchar *bookmarks_file = NULL;
static gchar *session_file = NULL;
static GtkNotebook *notebook = NULL;
static gboolean private_mode = FALSE;

static GPid server_pid = 0;
static gchar *server_path = NULL;
static gchar *venv_python = NULL;

static gchar *find_root_file(const gchar *name) {
    gchar *cwd = g_get_current_dir();
    if (!cwd)
        return NULL;
    gchar *path = g_build_filename(cwd, name, NULL);
    g_free(cwd);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        path = g_build_filename("/usr/local/share/openb", name, NULL);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_free(path);
            return NULL;
        }
    }
    return path;
}

static void ensure_python_env(void) {
    gchar *data_dir = g_build_filename(g_get_user_data_dir(), "openb", NULL);
    gchar *venv_dir = g_build_filename(data_dir, "venv", NULL);
    gchar *python = g_build_filename(venv_dir, "bin", "python", NULL);
    if (!g_file_test(python, G_FILE_TEST_EXISTS)) {
        gchar *cmd = g_strdup_printf("python3 -m venv '%s'", venv_dir);
        manager_run_command(cmd, NULL);
        g_free(cmd);
        gchar *pip = g_build_filename(venv_dir, "bin", "pip", NULL);
        cmd = g_strdup_printf("'%s' install -q requests", pip);
        manager_run_command(cmd, NULL);
        g_free(cmd);
        g_free(pip);
    }
    venv_python = python;
    g_free(venv_dir);
    g_free(data_dir);
}

static gboolean start_backend_server(void) {
    if (server_pid)
        return TRUE;
    ensure_python_env();
    server_path = find_root_file("server.py");
    if (!server_path || !venv_python)
        return FALSE;
    gchar *cmd = g_strdup_printf("'%s' '%s'", venv_python, server_path);
    gchar *dir = g_path_get_dirname(server_path);
    GError *err = NULL;
    gboolean ok = g_spawn_async(dir, (gchar *[]){"/bin/sh","-c",cmd,NULL}, NULL,
                               G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &server_pid, &err);
    g_free(dir);
    g_free(cmd);
    if (!ok) {
        manager_show_error_from_gerror(GTK_WINDOW(main_window), err);
        g_error_free(err);
        g_free(server_path);
        server_path = NULL;
        server_pid = 0;
        return FALSE;
    }
    return TRUE;
}

static void stop_backend_server(void) {
    if (server_pid) {
        kill(server_pid, SIGTERM);
        g_spawn_close_pid(server_pid);
        server_pid = 0;
    }
    g_clear_pointer(&server_path, g_free);
    g_clear_pointer(&venv_python, g_free);
}

static void new_tab(GtkWidget *w, gpointer d);
static void close_tab(GtkWidget *w, gpointer d);

static void adjust_window_size(void) {
    if (!main_window)
        return;
    GdkDisplay *display = gdk_display_get_default();
    if (!display)
        return;
    GdkMonitor *mon = gdk_display_get_primary_monitor(display);
    if (!mon)
        mon = gdk_display_get_monitor(display, 0);
    if (!mon)
        return;
    GdkRectangle geom;
    gdk_monitor_get_geometry(mon, &geom);
    gint w = geom.width * 0.9;
    gint h = geom.height * 0.9;
    gtk_window_resize(GTK_WINDOW(main_window), w, h);
    gtk_window_move(GTK_WINDOW(main_window),
        geom.x + (geom.width - w) / 2,
        geom.y + (geom.height - h) / 2);
}

static void window_realized(GtkWidget *w, gpointer data) {
    adjust_window_size();
}

static void load_extensions(WebKitUserContentManager *manager) {
    const gchar *dirs[] = {
        "./extensions",
        "/usr/local/share/openb/extensions",
        NULL
    };
    for (int i = 0; dirs[i]; i++) {
        GDir *d = g_dir_open(dirs[i], 0, NULL);
        if (!d)
            continue;
        const gchar *name;
        while ((name = g_dir_read_name(d))) {
            if (!g_str_has_suffix(name, ".js"))
                continue;
            gchar *path = g_build_filename(dirs[i], name, NULL);
            gchar *content = NULL;
            g_file_get_contents(path, &content, NULL, NULL);
            if (content) {
                WebKitUserScript *script = webkit_user_script_new(
                    content,
                    WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                    NULL,
                    NULL);
                webkit_user_content_manager_add_script(manager, script);
                webkit_user_script_unref(script);
                g_free(content);
            }
            g_free(path);
        }
        g_dir_close(d);
    }
}

static GtkWidget *popup_requested(WebKitWebView *view, WebKitNavigationAction *action, gpointer data) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
        "Popup blocked");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    return NULL;
}

static gboolean block_trackers(WebKitWebView *view, WebKitURIRequest *req,
                               WebKitURIResponse *redirected, gpointer data) {
    const gchar *uri = webkit_uri_request_get_uri(req);
    const gchar *blocked[] = {
        "google-analytics.com",
        "doubleclick.net",
        "adservice.google.com",
        NULL
    };
    for (int i = 0; blocked[i]; i++) {
        if (g_strstr_len(uri, -1, blocked[i])) {
            webkit_uri_request_set_uri(req, "about:blank");
            return FALSE;
        }
    }
    return FALSE;
}

static void safe_connect_send_request(WebKitWebResource *res) {
    if (!res)
        return;
    guint id = g_signal_lookup("send-request", G_OBJECT_TYPE(res));
    if (id == 0) {
        manager_log_warning("'send-request' not supported on %s",
            G_OBJECT_TYPE_NAME(res));
        return;
    }
    g_signal_connect(res, "send-request", G_CALLBACK(block_trackers), NULL);
}

static void resource_started(WebKitWebView *view, WebKitWebResource *res,
                             WebKitURIRequest *req, gpointer data) {
    safe_connect_send_request(res);
}

static gchar *home_file_uri = NULL;
static gchar *search_file_uri = NULL;

static gchar *find_data_file(const gchar *name) {
    gchar *cwd = g_get_current_dir();
    if (!cwd)
        return NULL;
    gchar *path = g_build_filename(cwd, "data", name, NULL);
    g_free(cwd);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        path = g_build_filename("/usr/local/share/openb", name, NULL);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_free(path);
            return NULL;
        }
    }
    return path;
}

static void load_home_page(void) {
    if (!home_file_uri) {
        gchar *path = find_data_file("home.html");
        if (!path) {
            GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_CLOSE,
                "Home page not found");
            gtk_dialog_run(GTK_DIALOG(d));
            gtk_widget_destroy(d);
            return;
        }
        home_file_uri = g_strdup_printf("file://%s", path);
        g_free(path);
    }
    if (home_file_uri)
        webkit_web_view_load_uri(web_view, home_file_uri);
}

static const gchar *get_search_uri(void) {
    if (!search_file_uri) {
        gchar *path = find_data_file("search.html");
        if (!path)
            return NULL;
        search_file_uri = g_strdup_printf("file://%s", path);
        g_free(path);
    }
    return search_file_uri;
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
        const gchar *search_base = get_search_uri();
        gchar *escaped = g_uri_escape_string(text, NULL, TRUE);
        if (search_base)
            uri = g_strdup_printf("%s?q=%s", search_base, escaped);
        else
            uri = g_strdup_printf("https://duckduckgo.com/?q=%s", escaped);
        g_free(escaped);
        search_history = g_slist_prepend(search_history, g_strdup(text));
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
        if (dark_mode) {
            webkit_web_view_evaluate_javascript(view,
                "document.documentElement.style.filter='invert(1) hue-rotate(180deg)';",
                -1, NULL, NULL, NULL, NULL, NULL);
        }
        if (!manager_check_memory_usage()) {
            manager_show_warning(GTK_WINDOW(main_window),
                "Memory usage is high. Consider closing some tabs.");
        }
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
    manager_show_error(GTK_WINDOW(main_window), "Failed to load %s:\n%s", uri, error->message);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Retry loading?");
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

static void on_new_private(GtkWidget *widget, gpointer data) {
    if (!program_path)
        return;
    gchar *cmd = g_strdup_printf("%s --private", program_path);
    g_spawn_command_line_async(cmd, NULL);
    g_free(cmd);
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

static void view_source(GtkWidget *widget, gpointer data) {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    if (uri && g_str_has_prefix(uri, "view-source:"))
        return;
    if (uri) {
        gchar *src = g_strconcat("view-source:", uri, NULL);
        webkit_web_view_load_uri(web_view, src);
        g_free(src);
    }
}

static void zoom_in(GtkWidget *widget, gpointer data) {
    zoom_level += 0.1;
    webkit_web_view_set_zoom_level(web_view, zoom_level);
}

static void zoom_out(GtkWidget *widget, gpointer data) {
    zoom_level -= 0.1;
    if (zoom_level < 0.5)
        zoom_level = 0.5;
    webkit_web_view_set_zoom_level(web_view, zoom_level);
}

static void reset_zoom(GtkWidget *widget, gpointer data) {
    zoom_level = 1.0;
    webkit_web_view_set_zoom_level(web_view, zoom_level);
}

static void toggle_javascript(GtkWidget *widget, gpointer data) {
    js_enabled = !js_enabled;
    webkit_settings_set_enable_javascript(webkit_web_view_get_settings(web_view), js_enabled);
    const gchar *msg = js_enabled ? "JavaScript Enabled" : "JavaScript Disabled";
    gtk_label_set_text(GTK_LABEL(status_label), msg);
}

static void clear_cache(GtkWidget *widget, gpointer data) {
    WebKitWebContext *ctx = webkit_web_view_get_context(web_view);
    webkit_web_context_clear_cache(ctx);
    gtk_label_set_text(GTK_LABEL(status_label), "Cache Cleared");
}

static void clear_cookies(GtkWidget *widget, gpointer data) {
    WebKitWebContext *ctx = webkit_web_view_get_context(web_view);
    WebKitWebsiteDataManager *dm = webkit_web_context_get_website_data_manager(ctx);
    webkit_website_data_manager_clear(dm,
        WEBKIT_WEBSITE_DATA_COOKIES,
        0,
        NULL,
        NULL,
        NULL);
    gtk_label_set_text(GTK_LABEL(status_label), "Cookies Cleared");
}

static void toggle_images(GtkWidget *widget, gpointer data) {
    images_enabled = !images_enabled;
    webkit_settings_set_auto_load_images(webkit_web_view_get_settings(web_view), images_enabled);
    const gchar *msg = images_enabled ? "Images Enabled" : "Images Disabled";
    gtk_label_set_text(GTK_LABEL(status_label), msg);
}

static void find_entry_changed(GtkEntry *entry, gpointer data) {
    const gchar *text = gtk_entry_get_text(entry);
    WebKitFindController *fc = webkit_web_view_get_find_controller(web_view);
    if (text && *text)
        webkit_find_controller_search(fc, text, WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE | WEBKIT_FIND_OPTIONS_WRAP_AROUND, G_MAXUINT);
    else
        webkit_find_controller_search_finish(fc);
}

static void open_find_dialog(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Find", GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    g_signal_connect(entry, "changed", G_CALLBACK(find_entry_changed), NULL);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    webkit_find_controller_search_finish(webkit_web_view_get_find_controller(web_view));
    gtk_widget_destroy(dialog);
}

static void screenshot_page(GtkWidget *widget, gpointer data) {
    GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(web_view));
    if (!win)
        return;
    gint w = gtk_widget_get_allocated_width(GTK_WIDGET(web_view));
    gint h = gtk_widget_get_allocated_height(GTK_WIDGET(web_view));
    GdkPixbuf *pb = gdk_pixbuf_get_from_window(win, 0, 0, w, h);
    if (!pb)
        return;
    gchar *fname = g_strdup_printf("%s/openb-screenshot.png", g_get_tmp_dir());
    gdk_pixbuf_save(pb, fname, "png", NULL, NULL);
    g_object_unref(pb);
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_CLOSE,
        "Screenshot saved to %s", fname);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    g_free(fname);
}

static void picture_in_picture(GtkWidget *widget, gpointer data) {
    const gchar *js = "var v=document.querySelector('video'); if(v){v.requestPictureInPicture();}";
    webkit_web_view_evaluate_javascript(web_view, js, -1, NULL, NULL, NULL, NULL, NULL);
}

static void show_search_history(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Search History",
        GTK_WINDOW(main_window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *list = gtk_list_box_new();
    for (GSList *l = search_history; l; l = l->next) {
        GtkWidget *row = gtk_label_new((const gchar *)l->data);
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
    }
    gtk_container_add(GTK_CONTAINER(box), list);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


static void downloads_window_show(void) {
    if (!downloads_window) {
        downloads_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(downloads_window), "Downloads");
        gtk_window_set_default_size(GTK_WINDOW(downloads_window), 400, 300);
        downloads_list = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(downloads_window), downloads_list);
    }
    gtk_widget_show_all(downloads_window);
}

static void download_progress(WebKitDownload *download, GParamSpec *pspec, gpointer user_data) {
    GtkProgressBar *bar = GTK_PROGRESS_BAR(user_data);
    gtk_progress_bar_set_fraction(bar, webkit_download_get_estimated_progress(download));
    BrowserTask *task = g_object_get_data(G_OBJECT(download), "bm_task");
    if (task) manager_update_task(task, webkit_download_get_estimated_progress(download));
}

static void download_finished(WebKitDownload *download, gpointer user_data) {
    GtkProgressBar *bar = GTK_PROGRESS_BAR(user_data);
    gtk_progress_bar_set_fraction(bar, 1.0);
    BrowserTask *task = g_object_get_data(G_OBJECT(download), "bm_task");
    if (task) manager_remove_task(task);
}

static void download_started(WebKitWebContext *ctx, WebKitDownload *download, gpointer user_data) {
    downloads_window_show();
    const char *uri = webkit_uri_request_get_uri(webkit_download_get_request(download));
    BrowserTask *task = manager_add_task(uri, TRUE, download);
    g_object_set_data(G_OBJECT(download), "bm_task", task);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *label = gtk_label_new(uri);
    GtkWidget *bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), bar, FALSE, FALSE, 0);
    gtk_list_box_insert(GTK_LIST_BOX(downloads_list), row, -1);
    gtk_widget_show_all(row);
    g_signal_connect(download, "notify::estimated-progress", G_CALLBACK(download_progress), bar);
    g_signal_connect(download, "finished", G_CALLBACK(download_finished), bar);

    const gchar *dest = webkit_download_get_destination(download);
    if (!dest) {
        const gchar *req = webkit_uri_request_get_uri(webkit_download_get_request(download));
        gchar *basename = g_path_get_basename(req);
        gchar *path = g_build_filename(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD), basename, NULL);
        gchar *uri_dest = g_strdup_printf("file://%s", path);
        webkit_download_set_destination(download, uri_dest);
        g_free(uri_dest);
        g_free(path);
        g_free(basename);
    }
}

static void open_extensions_page(GtkWidget *widget, gpointer data) {
    gchar *cwd = g_get_current_dir();
    gchar *path = g_build_filename(cwd, "data", "extensions.html", NULL);
    g_free(cwd);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        path = g_build_filename("/usr/local/share/openb", "extensions.html", NULL);
    }
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        gchar *uri = g_strdup_printf("file://%s", path);
        webkit_web_view_load_uri(web_view, uri);
        g_free(uri);
    }
    g_free(path);
}

static void show_tasks_manager(GtkWidget *widget, gpointer data) {
    GtkWidget *win = manager_get_window();
    gtk_widget_show_all(win);
    gtk_window_present(GTK_WINDOW(win));
}

static gboolean perform_update(void) {
    if (!g_find_program_in_path("curl") && !g_find_program_in_path("wget")) {
        GtkWidget *err = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "curl or wget is required for updating.");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return FALSE;
    }

    if (!program_path) {
        return FALSE;
    }

    GtkWidget *info = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        "Downloading OpenB update...\nThis may take a moment.");
    gtk_widget_show(info);
    while (gtk_events_pending())
        gtk_main_iteration();

    gchar *tmpdir = g_dir_make_tmp("openb-update-XXXXXX", NULL);
    if (!tmpdir) {
        gtk_widget_destroy(info);
        return FALSE;
    }

    gchar *archive = g_build_filename(tmpdir, "openb.tar.gz", NULL);
    const gchar *url = "https://github.com/Kgkkjjj/WebBrowser/releases/latest/download/openb.tar.gz";
    gchar *cmd;
    if (g_find_program_in_path("curl"))
        cmd = g_strdup_printf("curl -L '%s' -o '%s'", url, archive);
    else
        cmd = g_strdup_printf("wget -O '%s' '%s'", archive, url);

    GError *error = NULL;
    gboolean ok = manager_run_command(cmd, &error);
    g_free(cmd);

    if (!ok) {
        gtk_widget_destroy(info);
        GtkWidget *fail = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Download failed: %s", error ? error->message : "unknown error");
        gtk_dialog_run(GTK_DIALOG(fail));
        gtk_widget_destroy(fail);
        if (error)
            g_error_free(error);
        g_free(archive);
        g_rmdir(tmpdir);
        g_free(tmpdir);
        return FALSE;
    }

    gchar *extract_cmd = g_strdup_printf("tar -xzf '%s' -C '%s'", archive, tmpdir);
    ok = manager_run_command(extract_cmd, &error);
    g_free(extract_cmd);

    if (!ok) {
        gtk_widget_destroy(info);
        GtkWidget *fail = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Extraction failed: %s", error ? error->message : "unknown error");
        gtk_dialog_run(GTK_DIALOG(fail));
        gtk_widget_destroy(fail);
        if (error)
            g_error_free(error);
        g_free(archive);
        g_rmdir(tmpdir);
        g_free(tmpdir);
        return FALSE;
    }

    gchar *new_binary = g_build_filename(tmpdir, "openb", NULL);
    gchar *install_cmd = g_strdup_printf("install -m 755 '%s' '%s'", new_binary, program_path);
    ok = manager_run_command(install_cmd, &error);
    g_free(install_cmd);

    gtk_widget_destroy(info);

    g_free(archive);
    g_free(new_binary);
    g_remove(tmpdir);
    g_free(tmpdir);

    if (!ok) {
        GtkWidget *fail = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Install failed: %s", error ? error->message : "unknown error");
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
        "Download and install the latest OpenB release?");
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

static void copy_url(GtkWidget *widget, gpointer data) {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    if (!uri)
        return;
    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(cb, uri, -1);
    gtk_label_set_text(GTK_LABEL(status_label), "URL Copied");
}

static void paste_and_go(GtkWidget *widget, gpointer data) {
    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gchar *text = gtk_clipboard_wait_for_text(cb);
    if (text) {
        gtk_entry_set_text(url_entry, text);
        on_url_activate(url_entry, NULL);
        g_free(text);
    }
}

static void open_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
        GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (fname) {
            gchar *uri = g_filename_to_uri(fname, NULL, NULL);
            webkit_web_view_load_uri(web_view, uri);
            g_free(uri);
            g_free(fname);
        }
    }
    gtk_widget_destroy(dialog);
}

static void save_finished(GObject *src, GAsyncResult *res, gpointer data) {
    GError *error = NULL;
    gboolean ok = webkit_web_view_save_to_file_finish(WEBKIT_WEB_VIEW(src), res, &error);
    GtkWidget *d;
    if (!ok) {
        d = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            "Save failed: %s", error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
    } else {
        d = gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
            "Page saved.");
    }
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void save_page(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Page",
        GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "page.mhtml");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (fname) {
            GFile *file = g_file_new_for_path(fname);
            webkit_web_view_save_to_file(web_view, file,
                WEBKIT_SAVE_MODE_MHTML, NULL, save_finished, NULL);
            g_object_unref(file);
            g_free(fname);
        }
    }
    gtk_widget_destroy(dialog);
}

static void print_page(GtkWidget *widget, gpointer data) {
    WebKitPrintOperation *op = webkit_print_operation_new(web_view);
    webkit_print_operation_run_dialog(op, GTK_WINDOW(main_window));
    g_object_unref(op);
}

static void toggle_fullscreen(GtkWidget *widget, gpointer data) {
    if (is_fullscreen) {
        gtk_window_unfullscreen(GTK_WINDOW(main_window));
        is_fullscreen = FALSE;
    } else {
        gtk_window_fullscreen(GTK_WINDOW(main_window));
        is_fullscreen = TRUE;
    }
}


static void toggle_dark_mode(GtkWidget *widget, gpointer data) {
    dark_mode = !dark_mode;
    const gchar *js = dark_mode ?
        "document.documentElement.style.filter='invert(1) hue-rotate(180deg)';" :
        "document.documentElement.style.filter='';";
    webkit_web_view_evaluate_javascript(web_view, js, -1, NULL, NULL, NULL, NULL, NULL);
}

static void toggle_reader_mode(GtkWidget *w, gpointer d) {
    const gchar *script = "document.body.innerHTML='<article style=\"margin:2em;\">'+document.body.innerText+'</article>'";
    webkit_web_view_evaluate_javascript(web_view, script, -1, NULL, NULL, NULL, NULL, NULL);
}

static void page_info(GtkWidget *widget, gpointer data) {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    const gchar *title = webkit_web_view_get_title(web_view);
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "Title: %s\nURL: %s",
        title ? title : "", uri ? uri : "");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void clear_history(GtkWidget *widget, gpointer data) {
    g_slist_free_full(search_history, g_free);
    search_history = NULL;
    gtk_label_set_text(GTK_LABEL(status_label), "History Cleared");
}

static void clear_downloads(GtkWidget *widget, gpointer data) {
    if (!downloads_list)
        return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(downloads_list));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    gtk_label_set_text(GTK_LABEL(status_label), "Downloads Cleared");
}

static void load_bookmarks(void) {
    if (!bookmarks_file)
        return;
    gchar *content = NULL;
    g_file_get_contents(bookmarks_file, &content, NULL, NULL);
    if (!content)
        return;
    gchar **lines = g_strsplit(content, "\n", -1);
    for (gint i = 0; lines[i]; i++) {
        if (*lines[i])
            bookmarks = g_slist_prepend(bookmarks, g_strdup(lines[i]));
    }
    g_strfreev(lines);
    g_free(content);
}

static void save_bookmarks(void) {
    if (!bookmarks_file)
        return;
    GString *out = g_string_new("");
    for (GSList *l = bookmarks; l; l = l->next) {
        g_string_append(out, (const gchar *)l->data);
        g_string_append_c(out, '\n');
    }
    g_file_set_contents(bookmarks_file, out->str, -1, NULL);
    g_string_free(out, TRUE);
}

static void open_bookmark_url(GtkButton *button, gpointer d) {
    const gchar *url = g_object_get_data(G_OBJECT(button), "openb-url");
    if (url)
        webkit_web_view_load_uri(web_view, url);
}

static void bookmark_page(GtkWidget *w, gpointer d) {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    if (!uri)
        return;
    bookmarks = g_slist_prepend(bookmarks, g_strdup(uri));
    save_bookmarks();
    gtk_label_set_text(GTK_LABEL(status_label), "Bookmarked");
}

static void open_bookmarks(GtkWidget *w, gpointer d) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Bookmarks", GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *list = gtk_list_box_new();
    for (GSList *l = bookmarks; l; l = l->next) {
        const gchar *url = l->data;
        GtkWidget *row = gtk_button_new_with_label(url);
        g_object_set_data_full(G_OBJECT(row), "openb-url", g_strdup(url), g_free);
        g_signal_connect(row, "clicked", G_CALLBACK(open_bookmark_url), NULL);
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
    }
    gtk_container_add(GTK_CONTAINER(box), list);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void save_session(void) {
    if (!session_file)
        return;
    GString *out = g_string_new("");
    if (!notebook) {
        g_string_free(out, TRUE);
        return;
    }
    int pages = gtk_notebook_get_n_pages(notebook);
    for (int i = 0; i < pages; i++) {
        WebKitWebView *wv = WEBKIT_WEB_VIEW(gtk_notebook_get_nth_page(notebook, i));
        const gchar *uri = webkit_web_view_get_uri(wv);
        if (uri) {
            g_string_append(out, uri);
            g_string_append_c(out, '\n');
        }
    }
    g_file_set_contents(session_file, out->str, -1, NULL);
    g_string_free(out, TRUE);
}

static void load_session(void) {
    if (!session_file)
        return;
    gchar *content = NULL;
    if (!g_file_get_contents(session_file, &content, NULL, NULL) || !content)
        return;
    gchar **lines = g_strsplit(content, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        if (*lines[i]) {
            if (i == 0)
                webkit_web_view_load_uri(web_view, lines[i]);
            else {
                new_tab(NULL, NULL);
                webkit_web_view_load_uri(web_view, lines[i]);
            }
        }
    }
    g_strfreev(lines);
    g_free(content);
}

static void tab_switched(GtkNotebook *nb, GtkWidget *page, guint page_num, gpointer data) {
    web_view = WEBKIT_WEB_VIEW(page);
}

static void new_tab(GtkWidget *w, gpointer d) {
    WebKitWebView *view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    load_extensions(webkit_web_view_get_user_content_manager(view));
    webkit_settings_set_enable_developer_extras(webkit_web_view_get_settings(view), TRUE);
    gint n = gtk_notebook_append_page(notebook, GTK_WIDGET(view), gtk_label_new("Tab"));
    gtk_notebook_set_current_page(notebook, n);
    web_view = view;
    g_signal_connect(view, "load-changed", G_CALLBACK(load_changed), NULL);
    g_signal_connect(view, "notify::estimated-load-progress", G_CALLBACK(progress_changed), NULL);
    g_signal_connect(view, "load-failed", G_CALLBACK(load_failed), NULL);
    g_signal_connect(view, "mouse-target-changed", G_CALLBACK(mouse_target_changed), NULL);
    webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(view), FALSE);
    g_signal_connect(view, "resource-load-started",
        G_CALLBACK(resource_started), NULL);
    g_signal_connect(view, "create", G_CALLBACK(popup_requested), NULL);
}

static void close_tab(GtkWidget *w, gpointer d) {
    if (!notebook)
        return;
    gint page = gtk_notebook_get_current_page(notebook);
    if (gtk_notebook_get_n_pages(notebook) > 1) {
        GtkWidget *child = gtk_notebook_get_nth_page(notebook, page);
        gtk_notebook_remove_page(notebook, page);
        if (page > 0)
            gtk_notebook_set_current_page(notebook, page - 1);
        else
            gtk_notebook_set_current_page(notebook, 0);
        gtk_widget_destroy(child);
        GtkWidget *current = gtk_notebook_get_nth_page(notebook,
            gtk_notebook_get_current_page(notebook));
        web_view = WEBKIT_WEB_VIEW(current);
    }
}

int main(int argc, char *argv[]) {
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("Failed to initialize GTK.\n");
        return 1;
    }

    manager_init();
    manager_set_memory_limit(200 * 1024 * 1024); /* 200 MB */

    program_path = argv[0];

    start_backend_server();

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--private") == 0)
            private_mode = TRUE;
    }

    gchar *data_dir = g_build_filename(g_get_user_data_dir(), "openb", NULL);
    gchar *cache_dir = g_build_filename(g_get_user_cache_dir(), "openb", NULL);
    g_mkdir_with_parents(data_dir, 0755);
    bookmarks_file = g_build_filename(data_dir, "bookmarks.txt", NULL);
    session_file = g_build_filename(data_dir, "session.txt", NULL);
    load_bookmarks();
    WebKitWebsiteDataManager *manager = NULL;
    WebKitWebContext *context = NULL;
    if (!private_mode) {
        manager = webkit_website_data_manager_new(
            "base-data-directory", data_dir,
            "base-cache-directory", cache_dir,
            NULL);
        context = webkit_web_context_new_with_website_data_manager(manager);
    } else {
        context = webkit_web_context_new_ephemeral();
    }
    webkit_web_context_set_spell_checking_enabled(context, FALSE);
    webkit_web_context_set_cache_model(context, WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER);
    g_signal_connect(context, "download-started", G_CALLBACK(download_started), NULL);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    main_window = window;
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    gtk_window_set_title(GTK_WINDOW(window), "OpenB");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);

    GtkToolItem *back = gtk_tool_button_new(NULL, "Back");
    GtkWidget *back_icon = gtk_image_new_from_icon_name("go-previous", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(back), back_icon);
    gtk_widget_show(back_icon);
    GtkToolItem *forward = gtk_tool_button_new(NULL, "Forward");
    GtkWidget *forward_icon = gtk_image_new_from_icon_name("go-next", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(forward), forward_icon);
    gtk_widget_show(forward_icon);
    GtkToolItem *reload = gtk_tool_button_new(NULL, "Reload");
    GtkWidget *reload_icon = gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(reload), reload_icon);
    gtk_widget_show(reload_icon);
    GtkToolItem *stop = gtk_tool_button_new(NULL, "Stop");
    GtkWidget *stop_icon = gtk_image_new_from_icon_name("process-stop", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(stop), stop_icon);
    gtk_widget_show(stop_icon);
    GtkToolItem *home = gtk_tool_button_new(NULL, "Home");
    GtkWidget *home_icon = gtk_image_new_from_icon_name("go-home", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(home), home_icon);
    gtk_widget_show(home_icon);
    GtkToolItem *new_window = gtk_tool_button_new(NULL, "New Window");
    GtkWidget *newwin_icon = gtk_image_new_from_icon_name("window-new", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(new_window), newwin_icon);
    gtk_widget_show(newwin_icon);
    GtkToolItem *private_btn = gtk_tool_button_new(NULL, "Private Window");
    GtkWidget *priv_icon = gtk_image_new_from_icon_name("user-private", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(private_btn), priv_icon);
    gtk_widget_show(priv_icon);
    GtkToolItem *new_tab_btn = gtk_tool_button_new(NULL, "New Tab");
    GtkWidget *new_tab_icon = gtk_image_new_from_icon_name("tab-new", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(new_tab_btn), new_tab_icon);
    gtk_widget_show(new_tab_icon);
    GtkToolItem *close_tab_btn = gtk_tool_button_new(NULL, "Close Tab");
    GtkWidget *close_tab_icon = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(close_tab_btn), close_tab_icon);
    gtk_widget_show(close_tab_icon);
    GtkToolItem *update_btn = gtk_tool_button_new(NULL, "Update");
    GtkWidget *update_icon = gtk_image_new_from_icon_name("system-software-update", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(update_btn), update_icon);
    gtk_widget_show(update_icon);
    GtkToolItem *inspector_btn = gtk_tool_button_new(NULL, "DevTools");
    GtkWidget *inspector_icon = gtk_image_new_from_icon_name("applications-development", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(inspector_btn), inspector_icon);
    gtk_widget_show(inspector_icon);
    GtkToolItem *viewsrc_btn = gtk_tool_button_new(NULL, "View Source");
    GtkWidget *viewsrc_icon = gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(viewsrc_btn), viewsrc_icon);
    gtk_widget_show(viewsrc_icon);
    GtkToolItem *zoom_in_btn = gtk_tool_button_new(NULL, "Zoom In");
    GtkWidget *zoom_in_icon = gtk_image_new_from_icon_name("zoom-in", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(zoom_in_btn), zoom_in_icon);
    gtk_widget_show(zoom_in_icon);
    GtkToolItem *zoom_out_btn = gtk_tool_button_new(NULL, "Zoom Out");
    GtkWidget *zoom_out_icon = gtk_image_new_from_icon_name("zoom-out", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(zoom_out_btn), zoom_out_icon);
    gtk_widget_show(zoom_out_icon);
    GtkToolItem *reset_zoom_btn = gtk_tool_button_new(NULL, "Reset Zoom");
    GtkWidget *reset_icon = gtk_image_new_from_icon_name("zoom-original", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(reset_zoom_btn), reset_icon);
    gtk_widget_show(reset_icon);
    GtkToolItem *find_btn = gtk_tool_button_new(NULL, "Find");
    GtkWidget *find_icon = gtk_image_new_from_icon_name("edit-find", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(find_btn), find_icon);
    gtk_widget_show(find_icon);
    GtkToolItem *img_btn = gtk_tool_button_new(NULL, "Toggle Images");
    GtkWidget *img_icon = gtk_image_new_from_icon_name("image-x-generic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(img_btn), img_icon);
    gtk_widget_show(img_icon);
    GtkToolItem *cookies_btn = gtk_tool_button_new(NULL, "Clear Cookies");
    GtkWidget *cookie_icon = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(cookies_btn), cookie_icon);
    gtk_widget_show(cookie_icon);
    GtkToolItem *js_btn = gtk_tool_button_new(NULL, "Toggle JS");
    GtkWidget *js_icon = gtk_image_new_from_icon_name("applications-system", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(js_btn), js_icon);
    gtk_widget_show(js_icon);
    GtkToolItem *cache_btn = gtk_tool_button_new(NULL, "Clear Cache");
    GtkWidget *cache_icon = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(cache_btn), cache_icon);
    gtk_widget_show(cache_icon);
    GtkToolItem *shot_btn = gtk_tool_button_new(NULL, "Screenshot");
    GtkWidget *shot_icon = gtk_image_new_from_icon_name("camera-photo", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(shot_btn), shot_icon);
    gtk_widget_show(shot_icon);
    GtkToolItem *pip_btn = gtk_tool_button_new(NULL, "Picture-in-Picture");
    GtkWidget *pip_icon = gtk_image_new_from_icon_name("video-display", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(pip_btn), pip_icon);
    gtk_widget_show(pip_icon);
    GtkToolItem *downloads_btn = gtk_tool_button_new(NULL, "Downloads");
    GtkWidget *downloads_icon = gtk_image_new_from_icon_name("folder-download", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(downloads_btn), downloads_icon);
    gtk_widget_show(downloads_icon);
    GtkToolItem *history_btn = gtk_tool_button_new(NULL, "History");
    GtkWidget *history_icon = gtk_image_new_from_icon_name("document-open-recent", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(history_btn), history_icon);
    gtk_widget_show(history_icon);
    GtkToolItem *ext_btn = gtk_tool_button_new(NULL, "Extensions");
    GtkWidget *ext_icon = gtk_image_new_from_icon_name("preferences-system", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(ext_btn), ext_icon);
    gtk_widget_show(ext_icon);
    GtkToolItem *copy_btn = gtk_tool_button_new(NULL, "Copy URL");
    GtkWidget *copy_icon = gtk_image_new_from_icon_name("edit-copy", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(copy_btn), copy_icon);
    gtk_widget_show(copy_icon);
    GtkToolItem *paste_btn = gtk_tool_button_new(NULL, "Paste & Go");
    GtkWidget *paste_icon = gtk_image_new_from_icon_name("edit-paste", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(paste_btn), paste_icon);
    gtk_widget_show(paste_icon);
    GtkToolItem *bookmark_btn = gtk_tool_button_new(NULL, "Bookmark");
    GtkWidget *bm_icon = gtk_image_new_from_icon_name("bookmark-new", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(bookmark_btn), bm_icon);
    gtk_widget_show(bm_icon);
    GtkToolItem *bookmarks_btn = gtk_tool_button_new(NULL, "Bookmarks");
    GtkWidget *bms_icon = gtk_image_new_from_icon_name("user-bookmarks", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(bookmarks_btn), bms_icon);
    gtk_widget_show(bms_icon);
    GtkToolItem *open_btn = gtk_tool_button_new(NULL, "Open");
    GtkWidget *open_icon = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(open_btn), open_icon);
    gtk_widget_show(open_icon);
    GtkToolItem *save_btn = gtk_tool_button_new(NULL, "Save");
    GtkWidget *save_icon = gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(save_btn), save_icon);
    gtk_widget_show(save_icon);
    GtkToolItem *print_btn = gtk_tool_button_new(NULL, "Print");
    GtkWidget *print_icon = gtk_image_new_from_icon_name("document-print", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(print_btn), print_icon);
    gtk_widget_show(print_icon);
    GtkToolItem *fs_btn = gtk_tool_button_new(NULL, "Fullscreen");
    GtkWidget *fs_icon = gtk_image_new_from_icon_name("view-fullscreen", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(fs_btn), fs_icon);
    gtk_widget_show(fs_icon);
    GtkToolItem *dark_btn = gtk_tool_button_new(NULL, "Dark Mode");
    GtkWidget *dark_icon = gtk_image_new_from_icon_name("weather-many-clouds", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(dark_btn), dark_icon);
    gtk_widget_show(dark_icon);
    GtkToolItem *reader_btn = gtk_tool_button_new(NULL, "Reader Mode");
    GtkWidget *reader_icon = gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(reader_btn), reader_icon);
    gtk_widget_show(reader_icon);
    GtkToolItem *info_btn = gtk_tool_button_new(NULL, "Page Info");
    GtkWidget *info_icon = gtk_image_new_from_icon_name("dialog-information", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(info_btn), info_icon);
    gtk_widget_show(info_icon);
    GtkToolItem *clearhist_btn = gtk_tool_button_new(NULL, "Clear History");
    GtkWidget *chist_icon = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(clearhist_btn), chist_icon);
    gtk_widget_show(chist_icon);
    GtkToolItem *cleardl_btn = gtk_tool_button_new(NULL, "Clear Downloads");
    GtkWidget *cdl_icon = gtk_image_new_from_icon_name("edit-delete", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(cleardl_btn), cdl_icon);
    gtk_widget_show(cdl_icon);
    GtkToolItem *tasks_btn = gtk_tool_button_new(NULL, "Tasks");
    GtkWidget *tasks_icon = gtk_image_new_from_icon_name("utilities-system-monitor", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(tasks_btn), tasks_icon);
    gtk_widget_show(tasks_icon);
    GtkToolItem *about = gtk_tool_button_new(NULL, "About");
    GtkWidget *about_icon = gtk_image_new_from_icon_name("help-about", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(about), about_icon);
    gtk_widget_show(about_icon);
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
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), private_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_tab_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), close_tab_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), update_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), inspector_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), viewsrc_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), zoom_in_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), zoom_out_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reset_zoom_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), find_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), img_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), cookies_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), js_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), cache_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), shot_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pip_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), downloads_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), history_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), ext_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), copy_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), paste_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), bookmark_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), bookmarks_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), open_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), save_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), print_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), fs_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), dark_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reader_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), info_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), clearhist_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), cleardl_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tasks_btn, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), entry_item, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), about, -1);

    gtk_widget_set_tooltip_text(GTK_WIDGET(back), "Go Back");
    gtk_widget_set_tooltip_text(GTK_WIDGET(forward), "Go Forward");
    gtk_widget_set_tooltip_text(GTK_WIDGET(reload), "Reload Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(stop), "Stop Loading");
    gtk_widget_set_tooltip_text(GTK_WIDGET(home), "Home Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(new_window), "New Window");
    gtk_widget_set_tooltip_text(GTK_WIDGET(private_btn), "Private Window");
    gtk_widget_set_tooltip_text(GTK_WIDGET(new_tab_btn), "New Tab");
    gtk_widget_set_tooltip_text(GTK_WIDGET(close_tab_btn), "Close Tab");
    gtk_widget_set_tooltip_text(GTK_WIDGET(update_btn), "Update OpenB");
    gtk_widget_set_tooltip_text(GTK_WIDGET(inspector_btn), "Open Dev Tools");
    gtk_widget_set_tooltip_text(GTK_WIDGET(viewsrc_btn), "View Page Source");
    gtk_widget_set_tooltip_text(GTK_WIDGET(zoom_in_btn), "Zoom In");
    gtk_widget_set_tooltip_text(GTK_WIDGET(zoom_out_btn), "Zoom Out");
    gtk_widget_set_tooltip_text(GTK_WIDGET(reset_zoom_btn), "Reset Zoom");
    gtk_widget_set_tooltip_text(GTK_WIDGET(find_btn), "Find in Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(img_btn), "Toggle Images");
    gtk_widget_set_tooltip_text(GTK_WIDGET(cookies_btn), "Clear Cookies");
    gtk_widget_set_tooltip_text(GTK_WIDGET(js_btn), "Enable/Disable JavaScript");
    gtk_widget_set_tooltip_text(GTK_WIDGET(cache_btn), "Clear Cache");
    gtk_widget_set_tooltip_text(GTK_WIDGET(shot_btn), "Take Screenshot");
    gtk_widget_set_tooltip_text(GTK_WIDGET(pip_btn), "Picture in Picture");
    gtk_widget_set_tooltip_text(GTK_WIDGET(downloads_btn), "Show Downloads");
    gtk_widget_set_tooltip_text(GTK_WIDGET(history_btn), "Search History");
    gtk_widget_set_tooltip_text(GTK_WIDGET(ext_btn), "Manage Extensions");
    gtk_widget_set_tooltip_text(GTK_WIDGET(copy_btn), "Copy URL");
    gtk_widget_set_tooltip_text(GTK_WIDGET(paste_btn), "Paste and Go");
    gtk_widget_set_tooltip_text(GTK_WIDGET(bookmark_btn), "Bookmark Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(bookmarks_btn), "Show Bookmarks");
    gtk_widget_set_tooltip_text(GTK_WIDGET(open_btn), "Open File");
    gtk_widget_set_tooltip_text(GTK_WIDGET(save_btn), "Save Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(print_btn), "Print Page");
    gtk_widget_set_tooltip_text(GTK_WIDGET(fs_btn), "Toggle Fullscreen");
    gtk_widget_set_tooltip_text(GTK_WIDGET(dark_btn), "Toggle Dark Mode");
    gtk_widget_set_tooltip_text(GTK_WIDGET(reader_btn), "Reader Mode");
    gtk_widget_set_tooltip_text(GTK_WIDGET(info_btn), "Page Info");
    gtk_widget_set_tooltip_text(GTK_WIDGET(clearhist_btn), "Clear Search History");
    gtk_widget_set_tooltip_text(GTK_WIDGET(cleardl_btn), "Clear Downloads");
    gtk_widget_set_tooltip_text(GTK_WIDGET(tasks_btn), "Show Task Manager");

    gtk_widget_add_accelerator(GTK_WIDGET(reload), "clicked", accel, GDK_KEY_F5, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(home), "clicked", accel, GDK_KEY_F6, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(new_window), "clicked", accel, GDK_KEY_N, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(private_btn), "clicked", accel, GDK_KEY_N, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(new_tab_btn), "clicked", accel, GDK_KEY_t, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(close_tab_btn), "clicked", accel, GDK_KEY_w, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(inspector_btn), "clicked", accel, GDK_KEY_F12, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(viewsrc_btn), "clicked", accel, GDK_KEY_U, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(zoom_in_btn), "clicked", accel, GDK_KEY_plus, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(zoom_out_btn), "clicked", accel, GDK_KEY_minus, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(reset_zoom_btn), "clicked", accel, GDK_KEY_0, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(find_btn), "clicked", accel, GDK_KEY_f, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(img_btn), "clicked", accel, GDK_KEY_i, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(cookies_btn), "clicked", accel, GDK_KEY_Delete, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(js_btn), "clicked", accel, GDK_KEY_J, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(cache_btn), "clicked", accel, GDK_KEY_F9, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(shot_btn), "clicked", accel, GDK_KEY_P, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(pip_btn), "clicked", accel, GDK_KEY_i, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(downloads_btn), "clicked", accel, GDK_KEY_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(history_btn), "clicked", accel, GDK_KEY_H, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(ext_btn), "clicked", accel, GDK_KEY_E, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(copy_btn), "clicked", accel, GDK_KEY_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(paste_btn), "clicked", accel, GDK_KEY_V, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(bookmark_btn), "clicked", accel, GDK_KEY_D, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(bookmarks_btn), "clicked", accel, GDK_KEY_B, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(open_btn), "clicked", accel, GDK_KEY_O, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(save_btn), "clicked", accel, GDK_KEY_S, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(print_btn), "clicked", accel, GDK_KEY_P, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(fs_btn), "clicked", accel, GDK_KEY_F11, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(dark_btn), "clicked", accel, GDK_KEY_F2, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(reader_btn), "clicked", accel, GDK_KEY_R, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(info_btn), "clicked", accel, GDK_KEY_F3, 0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(clearhist_btn), "clicked", accel, GDK_KEY_H, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(cleardl_btn), "clicked", accel, GDK_KEY_D, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(GTK_WIDGET(tasks_btn), "clicked", accel, GDK_KEY_M, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(context));
    load_extensions(webkit_web_view_get_user_content_manager(web_view));
    webkit_settings_set_enable_developer_extras(webkit_web_view_get_settings(web_view), TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(web_view), FALSE);
    g_signal_connect(web_view, "resource-load-started",
        G_CALLBACK(resource_started), NULL);
    g_signal_connect(web_view, "create", G_CALLBACK(popup_requested), NULL);
    g_signal_connect(back, "clicked", G_CALLBACK(navigate_back), NULL);
    g_signal_connect(forward, "clicked", G_CALLBACK(navigate_forward), NULL);
    g_signal_connect(reload, "clicked", G_CALLBACK(reload_page), NULL);
    g_signal_connect(stop, "clicked", G_CALLBACK(stop_loading), NULL);
    g_signal_connect(home, "clicked", G_CALLBACK(navigate_home), NULL);
    g_signal_connect(new_window, "clicked", G_CALLBACK(on_new_window), NULL);
    g_signal_connect(private_btn, "clicked", G_CALLBACK(on_new_private), NULL);
    g_signal_connect(new_tab_btn, "clicked", G_CALLBACK(new_tab), NULL);
    g_signal_connect(close_tab_btn, "clicked", G_CALLBACK(close_tab), NULL);
    g_signal_connect(update_btn, "clicked", G_CALLBACK(check_for_updates), NULL);
    g_signal_connect(inspector_btn, "clicked", G_CALLBACK(toggle_inspector), NULL);
    g_signal_connect(viewsrc_btn, "clicked", G_CALLBACK(view_source), NULL);
    g_signal_connect(zoom_in_btn, "clicked", G_CALLBACK(zoom_in), NULL);
    g_signal_connect(zoom_out_btn, "clicked", G_CALLBACK(zoom_out), NULL);
    g_signal_connect(reset_zoom_btn, "clicked", G_CALLBACK(reset_zoom), NULL);
    g_signal_connect(find_btn, "clicked", G_CALLBACK(open_find_dialog), NULL);
    g_signal_connect(img_btn, "clicked", G_CALLBACK(toggle_images), NULL);
    g_signal_connect(cookies_btn, "clicked", G_CALLBACK(clear_cookies), NULL);
    g_signal_connect(js_btn, "clicked", G_CALLBACK(toggle_javascript), NULL);
    g_signal_connect(cache_btn, "clicked", G_CALLBACK(clear_cache), NULL);
    g_signal_connect(shot_btn, "clicked", G_CALLBACK(screenshot_page), NULL);
    g_signal_connect(pip_btn, "clicked", G_CALLBACK(picture_in_picture), NULL);
    g_signal_connect(downloads_btn, "clicked", G_CALLBACK(downloads_window_show), NULL);
    g_signal_connect(history_btn, "clicked", G_CALLBACK(show_search_history), NULL);
    g_signal_connect(ext_btn, "clicked", G_CALLBACK(open_extensions_page), NULL);
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(copy_url), NULL);
    g_signal_connect(paste_btn, "clicked", G_CALLBACK(paste_and_go), NULL);
    g_signal_connect(bookmark_btn, "clicked", G_CALLBACK(bookmark_page), NULL);
    g_signal_connect(bookmarks_btn, "clicked", G_CALLBACK(open_bookmarks), NULL);
    g_signal_connect(open_btn, "clicked", G_CALLBACK(open_file), NULL);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(save_page), NULL);
    g_signal_connect(print_btn, "clicked", G_CALLBACK(print_page), NULL);
    g_signal_connect(fs_btn, "clicked", G_CALLBACK(toggle_fullscreen), NULL);
    g_signal_connect(dark_btn, "clicked", G_CALLBACK(toggle_dark_mode), NULL);
    g_signal_connect(reader_btn, "clicked", G_CALLBACK(toggle_reader_mode), NULL);
    g_signal_connect(info_btn, "clicked", G_CALLBACK(page_info), NULL);
    g_signal_connect(clearhist_btn, "clicked", G_CALLBACK(clear_history), NULL);
    g_signal_connect(cleardl_btn, "clicked", G_CALLBACK(clear_downloads), NULL);
    g_signal_connect(tasks_btn, "clicked", G_CALLBACK(show_tasks_manager), NULL);
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

    notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(notebook, GTK_WIDGET(web_view), gtk_label_new("Home"));
    g_signal_connect(notebook, "switch-page", G_CALLBACK(tab_switched), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(notebook), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 0);
    status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "realize", G_CALLBACK(window_realized), NULL);
    load_session();
    if (!session_file || !g_file_test(session_file, G_FILE_TEST_EXISTS))
        load_home_page();

    gtk_widget_show_all(window);
    gtk_main();

    save_session();
    g_free(data_dir);
    g_free(cache_dir);
    g_free(bookmarks_file);
    g_free(session_file);
    g_free(home_file_uri);
    g_free(search_file_uri);
    if (manager)
        g_object_unref(manager);
    g_object_unref(context);
    stop_backend_server();
    manager_shutdown();
    return 0;
}
