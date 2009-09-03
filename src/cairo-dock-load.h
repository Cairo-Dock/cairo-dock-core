
#ifndef __CAIRO_DOCK_LOAD__
#define  __CAIRO_DOCK_LOAD__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-load.h A collection of surfaces and textures loader functions.
*/

void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription);
void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription);
CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription);

/** Say if 2 strings differ, taking into account NULL strings.
*/
#define cairo_dock_strings_differ(s1, s2) ((s1 && ! s2) || (! s1 && s2) || (s1 && s2 && strcmp (s1, s2)))
/** Say if 2 RGBA colors differ.
*/
#define cairo_dock_colors_rvb_differ(c1, c2) ((c1[0] != c2[0]) || (c1[1] != c2[1]) || (c1[2] != c2[2]))
/** Say if 2 RGB colors differ.
*/
#define cairo_dock_colors_differ(c1, c2) (cairo_dock_colors_rvb_differ (c1, c2) || (c1[3] != c2[3]))

/** Generate a complete path from a file name. '~' is handled, and files are supposed to be in the root folder of the current theme.
*@param cImageFile a file name or path. If it's already a path, it will just be duplicated.
*@return the path of the file.
*/
gchar *cairo_dock_generate_file_path (const gchar *cImageFile);

cairo_surface_t *cairo_dock_load_image (cairo_t *pSourceContext, const gchar *cImageFile, double *fImageWidth, double *fImageHeight, double fRotationAngle, double fAlpha, gboolean bReapeatAsPattern);

/*
*Cree la surface de reflection d'une icone (pour cairo).
*@param pSourceContext le contexte de dessin lie a la surface de l'icone; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_add_reflection_to_icon (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer);

/**Fill the image buffer (surface & texture) of a given icon, according to its type. Set its size accordingly, and fills the reflection buffer for cairo.
*@param icon the icon.
*@param pSourceContext a drawing context, not modified.
*@param fMaxScale maximum zoom.
*@param bHorizontalDock TRUE if the icon will be in a horizontal container (needed for the cairo reflect).
*@param bDirectionUp TRUE if the icon will be in a up container (needed for the cairo reflect).
*/
void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp);

/**Cut an UTF-8 or ASCII string to n characters, and add '...' to the end in cas it was effectively cut. It manages correctly UTF-8 strings.
*@param cString the string.
*@param iNbCaracters the maximum number of characters wished.
*@return the newly allocated string.
*/
gchar *cairo_dock_cut_string (const gchar *cString, guint iNbCaracters);

/**Fill the label buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*@param pSourceContext a drawing context, not modified.
*@param pTextDescription desctiption of the text rendering.
*/
void cairo_dock_fill_one_text_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription);

/**Fill the quick-info buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*@param pSourceContext a drawing context, not modified.
*@param pTextDescription desctiption of the text rendering.
*@param fMaxScale maximum zoom.
*/
void cairo_dock_fill_one_quick_info_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, double fMaxScale);

void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp);
#define cairo_dock_fill_icon_buffers_for_desklet(pIcon, pSourceContext) cairo_dock_fill_icon_buffers (pIcon, pSourceContext, 1, CAIRO_DOCK_HORIZONTAL, TRUE);
#define cairo_dock_fill_icon_buffers_for_dock(pIcon, pSourceContext, pDock) cairo_dock_fill_icon_buffers (pIcon, pSourceContext, 1 + myIcons.fAmplitude, pDock->bHorizontalDock, pDock->bDirectionUp);

/** Fill all the buffers (surfaces & textures) of a given icon, according to its type. Set its size accordingly, and fills the reflection buffer for cairo. Label and quick-info are loaded with the current global text description.
*@param pIcon the icon.
*@param pContainer its container.
*/
void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer);

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data);
#define cairo_dock_load_buffers_in_one_dock(pDock) cairo_dock_reload_buffers_in_dock (NULL, pDock, GINT_TO_POINTER (TRUE))

#define cairo_dock_reload_one_icon_buffer_in_dock_full(icon, pDock, pCairoContext) do {\
	icon->fWidth /= pDock->fRatio;\
	icon->fHeight /= pDock->fRatio;\
	cairo_dock_fill_one_icon_buffer (icon, pCairoContext, 1 + myIcons.fAmplitude, pDock->bHorizontalDock, pDock->bDirectionUp);\
	icon->fWidth *= pDock->fRatio;\
	icon->fHeight *= pDock->fRatio; } while (0)

void cairo_dock_reload_one_icon_buffer_in_dock (Icon *icon, CairoDock *pDock);



void cairo_dock_load_visible_zone (CairoDock *pDock, gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha);

cairo_surface_t *cairo_dock_load_stripes (cairo_t* pSourceContext, int iStripesWidth, int iStripesHeight, double fRotationAngle);

void cairo_dock_update_background_decorations_if_necessary (CairoDock *pDock, int iNewDecorationsWidth, int iNewDecorationsHeight);

void cairo_dock_load_background_decorations (CairoDock *pDock);

void cairo_dock_load_icons_background_surface (const gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale);

void cairo_dock_load_desktop_background_surface (void);
void cairo_dock_invalidate_desktop_bg_surface (void);
cairo_surface_t *cairo_dock_get_desktop_bg_surface (void);

void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, cairo_t* pSourceContext, double fMaxScale, double fIndicatorRatio);

void cairo_dock_load_active_window_indicator (cairo_t* pSourceContext, const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor);

void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, cairo_t* pSourceContext, double fMaxScale);


void cairo_dock_unload_additionnal_textures (void);


G_END_DECLS
#endif
