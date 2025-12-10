#include "gui.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

struct activate_ctx {
    const char *url;
};

static void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    gtk_main_quit();
}

static void on_reload_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    WebKitWebView *view = WEBKIT_WEB_VIEW(user_data);
    webkit_web_view_reload(view);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    struct activate_ctx *ctx = (struct activate_ctx *)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "LKJ Desktop IDE");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 900);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "LKJ – Local IDE");

    GtkWidget *reload = gtk_button_new_with_label("Reload");
    gtk_widget_set_tooltip_text(reload, "Refresh the IDE view");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), reload);

    GtkWidget *view = webkit_web_view_new();
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), ctx->url);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroller), view);

    GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(container), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), scroller, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), container);

    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(reload, "clicked", G_CALLBACK(on_reload_clicked), view);

    gtk_widget_show_all(window);
}

bool lkj_gui_supported(void) {
    return gtk_init_check(NULL, NULL);
}

int lkj_launch_gui(const char *url) {
    struct activate_ctx ctx = {.url = url};
    GtkApplication *app = gtk_application_new("com.lkj.desktop", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &ctx);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    return status;
}
