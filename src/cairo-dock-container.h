
#ifndef __CAIRO_DOCK_CONTAINER__
#define  __CAIRO_DOCK_CONTAINER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-container.h This class defines the basis of containers, that are classic or hardware accelerated animated windows.
*
* A container is a rectangular located surface, has the notion of orientation, can hold external datas, monitors the mouse position, and has its own animation loop.
*
* Docks, Desklets, Dialogs, and Flying-containers all derive from Containers.
*/

/// Main orientation of a container.
typedef enum {
	CAIRO_DOCK_VERTICAL = 0,
	CAIRO_DOCK_HORIZONTAL
	} CairoDockTypeHorizontality;

/// Types of available containers.
typedef enum {
	CAIRO_DOCK_TYPE_DOCK = 0,
	CAIRO_DOCK_TYPE_DESKLET,
	CAIRO_DOCK_TYPE_DIALOG,
	CAIRO_DOCK_TYPE_FLYING_CONTAINER
	} CairoDockTypeContainer;

/// Definition of a Container, whom derive Dock, Desklet, Dialog and FlyingContainer. 
struct _CairoContainer {
	/// type of container.
	CairoDockTypeContainer iType;
	/// window of the container.
	GtkWidget *pWidget;
	/// size of the container.
	gint iWidth, iHeight;
	/// position of the container.
	gint iWindowPositionX, iWindowPositionY;
	/// TURE is the mouse is inside the container (including the possible sub-widgets).
	gboolean bInside;
	/// TRUE if the container is horizontal, FALSE if vertical
	CairoDockTypeHorizontality bIsHorizontal;
	/// TRUE if the container is oriented upwards, FALSE if downwards.
	gboolean bDirectionUp;
#ifdef HAVE_GLITZ
	glitz_drawable_format_t *pDrawFormat;
	glitz_drawable_t* pGlitzDrawable;
	glitz_format_t* pGlitzFormat;
#else
	gpointer padding[3];
#endif
	/// External data.
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// ID of the timer of the animation.
	gint iSidGLAnimation;
	/// interval of time between 2 animation steps.
	gint iAnimationDeltaT;
	/// X position of the mouse in the container's system of reference.
	gint iMouseX;
	/// Y position of the mouse in the container's system of reference.
	gint iMouseY;
	/// zoom applied to the container.
	gdouble fRatio;
	/// TRUE if the container has a reflection power.
	gboolean bUseReflect;
	/// OpenGL context.
	GLXContext glContext;
	/// TRUE if an slow animation is running.
	gboolean bKeepSlowAnimation;
	/// counter for the animation.
	gint iAnimationStep;
	/// liste des notifications disponibles.
	GPtrArray *pNotificationsTab;
};

/// Get the Container part of a pointer.
#define CAIRO_CONTAINER(p) ((CairoContainer *) (p))

  /////////////
 // WINDOW //
///////////

GtkWidget *cairo_dock_create_container_window_full (gboolean bOpenGLWindow);

/** Create a GTK window that fits a CairoContainer (with transparency among others).
*@return the newly allocated GTK window.
*/
#define cairo_dock_create_container_window(...) cairo_dock_create_container_window_full (TRUE)
/** Same as above, but without an OpenGL context.
 */
#define cairo_dock_create_container_window_no_opengl(...) cairo_dock_create_container_window_full (FALSE)

/** Apply the scren colormap to a window, providing it transparency.
*@param pWidget a GTK window.
*/
void cairo_dock_set_colormap_for_window (GtkWidget *pWidget);
/** Apply the scren colormap to a container, providing it transparency, and activate Glitz if possible.
* @param pContainer the container.
*/
void cairo_dock_set_colormap (CairoContainer *pContainer);

  ////////////
 // REDRAW //
////////////

/** Clear and trigger the redraw of a Container.
*@param pContainer the Container to redraw.
*/
void cairo_dock_redraw_container (CairoContainer *pContainer);

/** Clear and trigger the redraw of a part of a container.
*@param pContainer the Container to redraw.
*@param pArea the zone to redraw.
*/
void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea);

/** Clear and trigger the redraw of an Icon. The drawing is not done immediately, but when the expose event is received.
*@param icon l'icone a retracer.
*@param pContainer le container de l'icone.
*/
void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer);



/** Search for the Container of a given Icon (dock or desklet in the case of an applet).
* @param icon the icon.
* @return the container contening this icon, or NULL if the icon is nowhere.
*/
CairoContainer *cairo_dock_search_container_from_icon (Icon *icon);


void cairo_dock_show_hide_container (CairoContainer *pContainer);


/** Let a widget accepts drag-and-drops.
* @param pWidget a widget.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

/** Say if a string is an adress (file://xxx, http://xxx, ftp://xxx, etc).
* @param cString a string.
*/
gboolean cairo_dock_string_is_adress (const gchar *cString);

/** Notify everybody that a drop has just occured.
* @param cReceivedData the dropped data.
* @param pPointedIcon the icon which was pointed when the drop occured.
* @param fOrder the order of the icon if the drop occured on it, or LAST_ORDER if the drop occured between 2 icons.
* @param pContainer the container of the icon
*/
void cairo_dock_notify_drop_data (gchar *cReceivedData, Icon *pPointedIcon, double fOrder, CairoContainer *pContainer);


/** Get the maximum zoom of ths icons inside a given container.
* @param pContainer the container.
* @return the maximum scale factor.
*/
#define cairo_dock_get_max_scale(pContainer) (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) : 1)


G_END_DECLS
#endif
