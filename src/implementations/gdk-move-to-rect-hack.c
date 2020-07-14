/*
 * gdk-move-to-rect-hack.c
 * 
 * Hack to provide the  gdk_window_move_to_rect() function on GTK 3.22.
 * This function was implemented internally on GTK 3.22, but was only
 * added to the public interface on GTK 3.24
 * 
 * based on gtk-layer-shell:
 * https://github.com/wmww/gtk-layer-shell
 * 
 */

#include "gdk-move-to-rect-hack.h"
#if (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 22)


typedef void (*MoveToRectFunc) (GdkWindow *window, const GdkRectangle *rect,
		GdkGravity rect_anchor, GdkGravity window_anchor, GdkAnchorHints anchor_hints,
		int rect_anchor_dx, int rect_anchor_dy);
                                
void gdk_window_move_to_rect_hacked(GdkWindow *window, const GdkRectangle *rect,
		GdkGravity rect_anchor, GdkGravity window_anchor, GdkAnchorHints anchor_hints,
		int rect_anchor_dx, int rect_anchor_dy)
{
	void *gdk_window_impl = *(void **)((char *)window + sizeof(GObject));
    void *gdk_window_impl_class = *(void **)gdk_window_impl;
	MoveToRectFunc *fptr = (MoveToRectFunc *)((char *)gdk_window_impl_class + sizeof(GObjectClass) + 10 * sizeof(void *));
	(*fptr) (window, rect, rect_anchor, window_anchor, anchor_hints, rect_anchor_dx, rect_anchor_dy);
}
#endif

