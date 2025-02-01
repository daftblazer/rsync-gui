#ifndef RSYNC_GTK_PROGRESS_WINDOW_H
#define RSYNC_GTK_PROGRESS_WINDOW_H

#include <gtk/gtk.h>

typedef struct _RsyncGtkProgressWindow RsyncGtkProgressWindow;

RsyncGtkProgressWindow* rsync_gtk_progress_window_new(GtkWindow *parent);
void rsync_gtk_progress_window_update(RsyncGtkProgressWindow *win, const char *text);
void rsync_gtk_progress_window_done(RsyncGtkProgressWindow *win);

#endif