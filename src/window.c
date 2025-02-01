#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *source_button;
    GtkWidget *dest_button;
    GtkWidget *delete_check;
    GtkWidget *dry_run_check;
    GtkWidget *sync_button;
    char *source_path;
    char *dest_path;
} RsyncGtkWindow;

typedef struct {
    RsyncGtkProgressWindow *progress_window;
    FILE *pipe;
    GIOChannel *channel;
    guint source_id;
} SyncData;

static void free_sync_data(SyncData *sync_data) {
    if (sync_data->channel) {
        g_io_channel_shutdown(sync_data->channel, TRUE, NULL);
        g_io_channel_unref(sync_data->channel);
    }
    if (sync_data->pipe) {
        pclose(sync_data->pipe);
    }
    g_free(sync_data);
}

static gboolean read_rsync_output(GIOChannel *channel, GIOCondition condition, gpointer user_data) {
    SyncData *sync_data = user_data;
    gchar *line;
    gsize length;
    GError *error = NULL;
    
    if (condition & G_IO_HUP) {
        rsync_gtk_progress_window_done(sync_data->progress_window);
        free_sync_data(sync_data);
        return G_SOURCE_REMOVE;
    }
    
    switch (g_io_channel_read_line(channel, &line, &length, NULL, &error)) {
        case G_IO_STATUS_NORMAL:
            rsync_gtk_progress_window_update(sync_data->progress_window, line);
            g_free(line);
            return G_SOURCE_CONTINUE;
            
        case G_IO_STATUS_ERROR:
            g_warning("Error reading rsync output: %s", error->message);
            g_error_free(error);
            // fallthrough
            
        case G_IO_STATUS_EOF:
        default:
            rsync_gtk_progress_window_done(sync_data->progress_window);
            free_sync_data(sync_data);
            return G_SOURCE_REMOVE;
    }
}

static void sync_clicked(G_GNUC_UNUSED GtkButton *button, RsyncGtkWindow *win) {
    if (!win->source_path || !win->dest_path) {
        GtkAlertDialog *dialog = gtk_alert_dialog_new("Please select both source and destination folders");
        gtk_alert_dialog_show(dialog, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
        g_object_unref(dialog);
        return;
    }

    char command[1024];
    char *source_with_slash;
    char *dest_with_slash;

    // Ensure source path has trailing slash
    if (win->source_path[strlen(win->source_path) - 1] != '/') {
        source_with_slash = g_strdup_printf("%s/", win->source_path);
    } else {
        source_with_slash = g_strdup(win->source_path);
    }

    // Ensure destination path has trailing slash
    if (win->dest_path[strlen(win->dest_path) - 1] != '/') {
        dest_with_slash = g_strdup_printf("%s/", win->dest_path);
    } else {
        dest_with_slash = g_strdup(win->dest_path);
    }

    g_print("Debug - Source path: '%s', Dest path: '%s'\n", source_with_slash, dest_with_slash);
    
    snprintf(command, sizeof(command), "rsync -av %s %s %s %s 2>&1",
             gtk_check_button_get_active(GTK_CHECK_BUTTON(win->dry_run_check)) ? "--dry-run" : "",
             gtk_check_button_get_active(GTK_CHECK_BUTTON(win->delete_check)) ? "--delete" : "",
             source_with_slash,
             dest_with_slash);

    g_free(source_with_slash);
    g_free(dest_with_slash);

    SyncData *sync_data = g_new0(SyncData, 1);
    sync_data->pipe = popen(command, "r");
    if (!sync_data->pipe) {
        GtkAlertDialog *dialog = gtk_alert_dialog_new("Error executing rsync command");
        gtk_alert_dialog_show(dialog, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
        g_object_unref(dialog);
        g_free(sync_data);
        return;
    }
    
    sync_data->progress_window = rsync_gtk_progress_window_new(
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
    
    // Set up IO channel to read rsync output
    int fd = fileno(sync_data->pipe);
    sync_data->channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(sync_data->channel, NULL, NULL);
    g_io_channel_set_flags(sync_data->channel,
                          g_io_channel_get_flags(sync_data->channel) | G_IO_FLAG_NONBLOCK,
                          NULL);
    
    sync_data->source_id = g_io_add_watch(sync_data->channel,
                                         G_IO_IN | G_IO_HUP,
                                         read_rsync_output,
                                         sync_data);
}

static void folder_selected_cb(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data) {
    RsyncGtkWindow *win = user_data;
    GtkWidget *button = g_object_get_data(G_OBJECT(dialog), "button");
    GError *error = NULL;
    
    GFile *file = gtk_file_dialog_select_folder_finish(dialog, result, &error);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            // Create path with trailing slash for display and storage
            char *path_with_slash;
            if (path[strlen(path) - 1] != '/') {
                path_with_slash = g_strdup_printf("%s/", path);
            } else {
                path_with_slash = g_strdup(path);
            }

            if (button == win->source_button) {
                g_free(win->source_path);
                win->source_path = g_strdup(path_with_slash);
                gtk_button_set_label(GTK_BUTTON(win->source_button), path_with_slash);
            } else if (button == win->dest_button) {
                g_free(win->dest_path);
                win->dest_path = g_strdup(path_with_slash);
                gtk_button_set_label(GTK_BUTTON(win->dest_button), path_with_slash);
            }
            
            g_free(path_with_slash);
            g_free(path);
        }
        g_object_unref(file);
    }
    
    if (error) {
        g_warning("Error selecting folder: %s", error->message);
        g_error_free(error);
    }
}

static void show_folder_dialog(GtkButton *button, RsyncGtkWindow *win) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Folder");
    gtk_file_dialog_set_modal(dialog, TRUE);
    g_object_set_data(G_OBJECT(dialog), "button", button);
    
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    gtk_file_dialog_select_folder(dialog,
                                parent,
                                NULL,
                                (GAsyncReadyCallback)folder_selected_cb,
                                win);
}

void rsync_gtk_window_init(GtkWidget *window) {
    RsyncGtkWindow *win = g_new0(RsyncGtkWindow, 1);
    g_object_set_data_full(G_OBJECT(window), "win-data", win, g_free);
    
    gtk_window_set_title(GTK_WINDOW(window), "Rsync GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 250);  // Reduced height since we don't need as much space

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);

    // Source folder button
    win->source_button = gtk_button_new_with_label("Select Source Folder");
    g_signal_connect(win->source_button, "clicked",
        G_CALLBACK(show_folder_dialog), win);
    gtk_box_append(GTK_BOX(box), win->source_button);

    // Destination folder button
    win->dest_button = gtk_button_new_with_label("Select Destination Folder");
    g_signal_connect(win->dest_button, "clicked",
        G_CALLBACK(show_folder_dialog), win);
    gtk_box_append(GTK_BOX(box), win->dest_button);

    // Options
    win->delete_check = gtk_check_button_new_with_label("Delete extraneous files from destination");
    gtk_box_append(GTK_BOX(box), win->delete_check);

    win->dry_run_check = gtk_check_button_new_with_label("Dry run (show what would be transferred)");
    gtk_box_append(GTK_BOX(box), win->dry_run_check);

    // Sync button
    win->sync_button = gtk_button_new_with_label("Start Sync");
    gtk_widget_set_margin_top(win->sync_button, 6);  // Add some extra spacing above the sync button
    g_signal_connect(win->sync_button, "clicked",
        G_CALLBACK(sync_clicked), win);
    gtk_box_append(GTK_BOX(box), win->sync_button);

    gtk_window_set_child(GTK_WINDOW(window), box);
}