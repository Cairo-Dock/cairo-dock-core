
#ifndef __CAIRO_DOCK_CALLBACKS__
#define  __CAIRO_DOCK_CALLBACKS__

#include <gtk/gtk.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_freeze_docks (gboolean bFreeze);

gboolean cairo_dock_render_dock_notification (gpointer pUserData, CairoDock *pDock, cairo_t *pCairoContext);
gboolean cairo_dock_on_expose (GtkWidget *pWidget, GdkEventExpose *pExpose, CairoDock *pDock);

void cairo_dock_show_subdock (Icon *pPointedIcon, gboolean bUpdate, CairoDock *pDock);
void cairo_dock_on_change_icon (Icon *pLastPointedIcon, Icon *pPointedIcon, CairoDock *pDock);
gboolean cairo_dock_on_motion_notify (GtkWidget* pWidget,
					GdkEventMotion* pMotion,
					CairoDock *pDock);


gboolean cairo_dock_emit_signal_on_dock (CairoDock *pDock, const gchar *cSignal);
gboolean cairo_dock_emit_leave_signal (CairoDock *pDock);
gboolean cairo_dock_emit_enter_signal (CairoDock *pDock);

gboolean cairo_dock_poll_screen_edge (CairoDock *pDock);

void cairo_dock_leave_from_main_dock (CairoDock *pDock);
gboolean cairo_dock_on_leave_notify (GtkWidget* pWidget,
					GdkEventCrossing* pEvent,
					CairoDock *pDock);

gboolean cairo_dock_on_enter_notify (GtkWidget* pWidget,
					GdkEventCrossing* pEvent,
					CairoDock *pDock);


gboolean cairo_dock_on_key_release (GtkWidget *pWidget,
				GdkEventKey *pKey,
				CairoDock *pDock);

gchar *cairo_dock_launch_command_sync (const gchar *cCommand);

gboolean cairo_dock_launch_command_full (const gchar *cCommand, gchar *cWorkingDirectory, ...);
#define cairo_dock_launch_command(cCommand,...) cairo_dock_launch_command_full (cCommand, NULL, ##__VA_ARGS__)

gboolean cairo_dock_notification_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, guint iButtonState);
gboolean cairo_dock_notification_middle_click_icon (gpointer pUserData, Icon *icon, CairoDock *pDock);
gboolean cairo_dock_on_button_press (GtkWidget* pWidget,
					GdkEventButton* pButton,
					CairoDock *pDock);

gboolean cairo_dock_notification_scroll_icon (gpointer pUserData, Icon *icon, CairoDock *pDock, int iDirection);
gboolean cairo_dock_on_scroll (GtkWidget* pWidget,
				GdkEventScroll* pScroll,
				CairoDock *pDock);


gboolean cairo_dock_on_configure (GtkWidget* pWidget,
	   			GdkEventConfigure* pEvent,
	   			CairoDock *pDock);


void cairo_dock_on_drag_data_received (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, CairoDock *pDock);
gboolean cairo_dock_notification_drop_data (gpointer pUserData, const gchar *cReceivedData, Icon *icon, double fOrder, CairoContainer *pContainer);

void cairo_dock_on_drag_motion (GtkWidget *pWidget, GdkDragContext *dc, gint x, gint y, guint t, CairoDock *pDock);
void cairo_dock_on_drag_leave (GtkWidget *pWidget, GdkDragContext *dc, guint time, CairoDock *pDock);


//gboolean cairo_dock_on_delete (GtkWidget *pWidget, GdkEvent *event, CairoDock *pDock);


void cairo_dock_show_dock_at_mouse (CairoDock *pDock);
void cairo_dock_raise_from_keyboard (const char *cKeyShortcut, gpointer data);

void cairo_dock_hide_dock_like_a_menu (void);

void cairo_dock_unregister_current_flying_container (void);

G_END_DECLS
#endif
