#include <gtk/gtk.h>
#include "window.h"

static void activate(GtkApplication *app, G_GNUC_UNUSED gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    rsync_gtk_window_init(window);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.example.rsync-gtk",
                                            G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}