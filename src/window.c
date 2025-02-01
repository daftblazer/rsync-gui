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
    GtkWidget *output_text;
    char *source_path;
    char *dest_path;
} RsyncGtkWindow;

static void sync_clicked(G_GNUC_UNUSED GtkButton *button, RsyncGtkWindow *win) {
    if (!win->source_path || !win->dest_path) {
        gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->output_text)),
            "Please select both source and destination folders",
            -1);
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

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->output_text)),
            "Error executing rsync command",
            -1);
        return;
    }

    char buffer[1024];
    GString *output = g_string_new(NULL);
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        g_string_append(output, buffer);
    }
    pclose(pipe);

    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->output_text)),
        output->str,
        -1);
    g_string_free(output, TRUE);
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
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);

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
    g_signal_connect(win->sync_button, "clicked",
        G_CALLBACK(sync_clicked), win);
    gtk_box_append(GTK_BOX(box), win->sync_button);

    // Output text view
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    win->output_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(win->output_text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(win->output_text), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), win->output_text);
    gtk_box_append(GTK_BOX(box), scrolled);

    gtk_window_set_child(GTK_WINDOW(window), box);
}