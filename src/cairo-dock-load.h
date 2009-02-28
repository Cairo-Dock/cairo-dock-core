
#ifndef __CAIRO_DOCK_LOAD__
#define  __CAIRO_DOCK_LOAD__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription);
void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription);
CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription);

#define cairo_dock_strings_differ(s1, s2) ((s1 && ! s2) || (! s1 && s2) || (s1 && s2 && strcmp (s1, s2)))
#define cairo_dock_colors_rvb_differ(c1, c2) ((c1[0] != c2[0]) || (c1[1] != c2[1]) || (c1[2] != c2[2]))
#define cairo_dock_colors_differ(c1, c2) (cairo_dock_colors_rvb_differ (c1, c2) || (c1[3] != c2[3]))

gchar *cairo_dock_generate_file_path (const gchar *cImageFile);

cairo_surface_t *cairo_dock_load_image (cairo_t *pSourceContext, const gchar *cImageFile, double *fImageWidth, double *fImageHeight, double fRotationAngle, double fAlpha, gboolean bReapeatAsPattern);
cairo_surface_t *cairo_dock_load_image_for_icon (cairo_t *pSourceContext, const gchar *cImageFile, double fImageWidth, double fImageHeight);
#define cairo_dock_load_image_for_square_icon(pSourceContext, cImageFile, fImageSize) cairo_dock_load_image_for_icon (pSourceContext, cImageFile, fImageSize, fImageSize)

void cairo_dock_load_reflect_on_icon (Icon *icon, cairo_t *pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp);
void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp);

gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters);
void cairo_dock_fill_one_text_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription);

void cairo_dock_fill_one_quick_info_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, double fMaxScale);


void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp);
#define cairo_dock_fill_icon_buffers_for_desklet(pIcon, pSourceContext) cairo_dock_fill_icon_buffers (pIcon, pSourceContext, 1, CAIRO_DOCK_HORIZONTAL, TRUE);
#define cairo_dock_fill_icon_buffers_for_dock(pIcon, pSourceContext, pDock) cairo_dock_fill_icon_buffers (pIcon, pSourceContext, 1 + myIcons.fAmplitude, pDock->bHorizontalDock, pDock->bDirectionUp);

void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer);

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data);
#define cairo_dock_load_buffers_in_one_dock(pDock) cairo_dock_reload_buffers_in_dock (NULL, pDock, GINT_TO_POINTER (TRUE))


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


G_END_DECLS
#endif
