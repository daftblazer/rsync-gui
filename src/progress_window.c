#include "progress_window.h"
#include <stdio.h>

struct _RsyncGtkProgressWindow {
    GtkWindow *window;
    GtkWidget *progress_bar;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    guint pulse_id;
};

static gboolean pulse_progress(gpointer user_data) {
    RsyncGtkProgressWindow *win = user_data;
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(win->progress_bar));
    return G_SOURCE_CONTINUE;
}

RsyncGtkProgressWindow* rsync_gtk_progress_window_new(GtkWindow *parent) {
    RsyncGtkProgressWindow *win = g_new0(RsyncGtkProgressWindow, 1);
    
    // Create window
    win->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(win->window, "Sync Progress");
    gtk_window_set_default_size(win->window, 500, 300);
    gtk_window_set_transient_for(win->window, parent);
    gtk_window_set_modal(win->window, TRUE);
    
    // Create main box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    
    // Progress bar
    win->progress_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(box), win->progress_bar);
    
    // Scrolled text view
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    win->text_view = gtk_text_view_new();
    win->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(win->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(win->text_view), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), win->text_view);
    gtk_box_append(GTK_BOX(box), scrolled);
    
    gtk_window_set_child(win->window, box);
    gtk_window_present(win->window);
    
    // Start progress bar animation
    win->pulse_id = g_timeout_add(100, pulse_progress, win);
    
    return win;
}

void rsync_gtk_progress_window_update(RsyncGtkProgressWindow *win, const char *text) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(win->buffer, &end);
    gtk_text_buffer_insert(win->buffer, &end, text, -1);
    
    // Scroll to bottom
    gtk_text_buffer_get_end_iter(win->buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(win->text_view), &end, 0.0, FALSE, 0.0, 0.0);
}

void rsync_gtk_progress_window_done(RsyncGtkProgressWindow *win) {
    // Stop progress bar animation
    if (win->pulse_id > 0) {
        g_source_remove(win->pulse_id);
        win->pulse_id = 0;
    }
    
    // Set progress bar to 100%
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(win->progress_bar), 1.0);
    
    // Add done message
    rsync_gtk_progress_window_update(win, "\nSync completed!");
    
    // Add close button
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    GtkWidget *close_button = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_button, "clicked",
                            G_CALLBACK(gtk_window_destroy), win->window);
    gtk_box_append(GTK_BOX(button_box), close_button);
    gtk_box_append(GTK_BOX(gtk_window_get_child(win->window)), button_box);
    
    // Free the window structure when the window is closed
    g_signal_connect_swapped(win->window, "destroy",
                            G_CALLBACK(g_free), win);
}