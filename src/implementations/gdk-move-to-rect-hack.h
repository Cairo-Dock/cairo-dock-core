/*
 * gdk-move-to-rect-hack.h
 * 
 * Hack to provide the  gdk_window_move_to_rect() function on GTK 3.22.
 * This function was implemented internally on GTK 3.22, but was only
 * added to the public interface on GTK 3.24
 * 
 */


#ifndef GDK_MOVE_TO_RECT_HACK_H
#define GDK_MOVE_TO_RECT_HACK_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#if (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 22)

void gdk_window_move_to_rect_hacked (GdkWindow *window, const GdkRectangle *rect,
		GdkGravity rect_anchor, GdkGravity window_anchor, GdkAnchorHints anchor_hints,
		int rect_anchor_dx, int rect_anchor_dy);
#define gdk_window_move_to_rect gdk_window_move_to_rect_hacked

#endif
#endif

