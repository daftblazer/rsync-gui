#include "progress_window.h"
#include <stdio.h>

// Model for file operations
typedef struct {
    GObject parent_instance;
    char *action;
    char *file;
} FileOperation;

typedef struct {
    GObjectClass parent_class;
} FileOperationClass;

G_DEFINE_TYPE(FileOperation, file_operation, G_TYPE_OBJECT)

static void file_operation_finalize(GObject *object) {
    FileOperation *self = (FileOperation *)object;
    g_free(self->action);
    g_free(self->file);
    G_OBJECT_CLASS(file_operation_parent_class)->finalize(object);
}

static void file_operation_class_init(FileOperationClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->finalize = file_operation_finalize;
}

static void file_operation_init(FileOperation *self) {
    self->action = NULL;
    self->file = NULL;
}

struct _RsyncGtkProgressWindow {
    GtkWindow *window;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    GtkWidget *files_list;
    GListStore *list_store;
    guint pulse_id;
};

static gboolean pulse_progress(gpointer user_data) {
    RsyncGtkProgressWindow *win = user_data;
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(win->progress_bar));
    return G_SOURCE_CONTINUE;
}

static void setup_list_item(GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                          GtkListItem *list_item,
                          gpointer user_data G_GNUC_UNUSED) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_list_item_set_child(list_item, box);

    GtkWidget *action_label = gtk_label_new(NULL);
    gtk_widget_set_halign(action_label, GTK_ALIGN_START);
    gtk_widget_set_size_request(action_label, 80, -1);
    
    GtkWidget *file_label = gtk_label_new(NULL);
    gtk_widget_set_halign(file_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(file_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(file_label), 0.0);
    
    gtk_box_append(GTK_BOX(box), action_label);
    gtk_box_append(GTK_BOX(box), file_label);
}

static void bind_list_item(GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                          GtkListItem *list_item,
                          gpointer user_data G_GNUC_UNUSED) {
    GtkWidget *box = gtk_list_item_get_child(list_item);
    if (!box) return;

    GtkWidget *action_label = gtk_widget_get_first_child(box);
    if (!action_label) return;

    GtkWidget *file_label = gtk_widget_get_next_sibling(action_label);
    if (!file_label) return;
    
    FileOperation *op = gtk_list_item_get_item(list_item);
    if (!op) return;

    // Set text
    gtk_label_set_text(GTK_LABEL(action_label), op->action);
    gtk_label_set_text(GTK_LABEL(file_label), op->file);

    // Apply colors using CSS classes
    if (g_str_equal(op->action, "deleting")) {
        gtk_widget_add_css_class(action_label, "error");
        gtk_widget_add_css_class(file_label, "error");
    } else {
        gtk_widget_add_css_class(action_label, "success");
        gtk_widget_add_css_class(file_label, "success");
    }
}

static void unbind_list_item(GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                            GtkListItem *list_item,
                            gpointer user_data G_GNUC_UNUSED) {
    GtkWidget *box = gtk_list_item_get_child(list_item);
    if (!box) return;

    GtkWidget *action_label = gtk_widget_get_first_child(box);
    if (!action_label) return;

    GtkWidget *file_label = gtk_widget_get_next_sibling(action_label);
    if (!file_label) return;
    
    // Remove all CSS classes
    gtk_widget_remove_css_class(action_label, "error");
    gtk_widget_remove_css_class(action_label, "success");
    gtk_widget_remove_css_class(file_label, "error");
    gtk_widget_remove_css_class(file_label, "success");
}

RsyncGtkProgressWindow* rsync_gtk_progress_window_new(GtkWindow *parent) {
    g_return_val_if_fail(GTK_IS_WINDOW(parent), NULL);

    RsyncGtkProgressWindow *win = g_new0(RsyncGtkProgressWindow, 1);
    
    // Create window
    win->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(win->window, "Sync Progress");
    gtk_window_set_default_size(win->window, 600, 400);
    gtk_window_set_transient_for(win->window, parent);
    gtk_window_set_modal(win->window, TRUE);
    
    // Create main box
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);

    // Status label with monospace font
    win->status_label = gtk_label_new("Preparing to sync...");
    gtk_widget_set_halign(win->status_label, GTK_ALIGN_START);
    PangoAttrList *attr_list = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_family_new("monospace");
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(win->status_label), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_box_append(GTK_BOX(box), win->status_label);
    
    // Progress bar
    win->progress_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(box), win->progress_bar);
    
    // Create list model and view
    win->list_store = g_list_store_new(file_operation_get_type());
    
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_list_item), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_list_item), NULL);
    g_signal_connect(factory, "unbind", G_CALLBACK(unbind_list_item), NULL);

    GtkSelectionModel *selection_model = 
        GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(win->list_store)));
    
    win->files_list = gtk_list_view_new(selection_model, factory);
    gtk_widget_set_vexpand(win->files_list, TRUE);

    // Add CSS provider for colors
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "label.error { color: #e01b24; }"
        "label.success { color: #2ec27e; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    // Add list view to scrolled window
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), win->files_list);
    gtk_box_append(GTK_BOX(box), scrolled);
    
    gtk_window_set_child(GTK_WINDOW(win->window), box);
    
    // Start progress bar animation
    win->pulse_id = g_timeout_add(100, pulse_progress, win);

    gtk_window_present(win->window);
    
    return win;
}

void rsync_gtk_progress_window_update(RsyncGtkProgressWindow *win, const char *text) {
    g_return_if_fail(win != NULL);
    g_return_if_fail(text != NULL);

    // Skip empty lines and standard rsync output lines
    if (text[0] == '\0' || text[0] == '\n' ||
        strstr(text, "building file list") ||
        strstr(text, "files to consider") ||
        strstr(text, "total size is")) {
        return;
    }

    char *action = NULL;
    char *file = NULL;

    // First check for specific rsync prefixes
    if (g_str_has_prefix(text, "sending ")) {
        action = g_strdup("sending");
        file = g_strdup(text + 8);
    } else if (g_str_has_prefix(text, "deleting ")) {
        action = g_strdup("deleting");
        file = g_strdup(text + 9);
    } else if (g_str_has_prefix(text, "receiving ")) {
        action = g_strdup("receiving");
        file = g_strdup(text + 10);
    } else {
        // If no prefix, treat as a file being sent
        // Skip certain rsync messages
        if (!strstr(text, "incremental file list") &&
            !strstr(text, "bytes/sec") &&
            !strstr(text, "to-check=") &&
            !strstr(text, "sent ") &&
            !strstr(text, "received ")) {
            action = g_strdup("sending");
            file = g_strdup(text);
        }
    }

    if (action && file) {
        // Remove trailing newline and spaces
        g_strstrip(file);

        // Update status label
        char *status_text = g_strdup_printf("%s: %s", action, file);
        gtk_label_set_text(GTK_LABEL(win->status_label), status_text);
        g_free(status_text);

        // Create new file operation
        FileOperation *op = g_object_new(file_operation_get_type(), NULL);
        op->action = action;
        op->file = file;

        // Add to list store
        g_list_store_insert(win->list_store, 0, op);
        g_object_unref(op);

        // Limit list to recent 1000 items for performance
        guint n_items = g_list_model_get_n_items(G_LIST_MODEL(win->list_store));
        if (n_items > 1000) {
            g_list_store_remove(win->list_store, n_items - 1);
        }
    } else {
        g_free(action);
        g_free(file);
    }
}

void rsync_gtk_progress_window_done(RsyncGtkProgressWindow *win) {
    g_return_if_fail(win != NULL);

    // Stop progress bar animation
    if (win->pulse_id > 0) {
        g_source_remove(win->pulse_id);
        win->pulse_id = 0;
    }
    
    // Set progress bar to 100%
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(win->progress_bar), 1.0);
    
    // Update status label
    gtk_label_set_text(GTK_LABEL(win->status_label), "Sync completed!");
    
    // Add close button
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(button_box, 12);
    
    GtkWidget *close_button = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(close_button, "suggested-action");
    g_signal_connect_swapped(close_button, "clicked",
                            G_CALLBACK(gtk_window_destroy), win->window);
    gtk_box_append(GTK_BOX(button_box), close_button);
    gtk_box_append(GTK_BOX(gtk_window_get_child(win->window)), button_box);
    
    // Free the window structure when the window is closed
    g_signal_connect_swapped(win->window, "destroy",
                            G_CALLBACK(g_free), win);
}