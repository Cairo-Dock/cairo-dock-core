/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by ChAnGFu (for any bug report, please mail me to changfu@cairo-dock.org)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-config.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-emblem.h"

extern gboolean g_bDisplayDropEmblem;

static CairoDockFullEmblem s_pFullEmblems[CAIRO_DOCK_EMBLEM_CLASSIC_NB];
static gchar *s_cEmblemConfPath[CAIRO_DOCK_EMBLEM_CLASSIC_NB];


//Fonctions proposées par Nécropotame, rédigées par ChAnGFu
void cairo_dock_draw_emblem_on_my_icon (cairo_t *pIconContext, const gchar *cIconFile, Icon *pIcon, CairoContainer *pContainer, CairoDockEmblem iEmblemType, gboolean bPersistent)
{
	cd_debug ("%s (%s %d)", __func__, cIconFile, iEmblemType);
	g_return_if_fail (pIcon != NULL && pContainer != NULL); 
	
	if (cIconFile == NULL) 
		return;
	
	double fImgX, fImgY, fImgW, fImgH, emblemW = pIcon->fWidth / 3, emblemH = pIcon->fHeight / 3;
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_surface_t *pCairoSurface = cairo_dock_create_surface_from_image (cIconFile, pIconContext, fMaxScale, emblemW, emblemH, CAIRO_DOCK_KEEP_RATIO, &fImgW, &fImgH, NULL, NULL);
	cairo_dock_draw_emblem_from_surface (pIconContext, pCairoSurface, pIcon, pContainer, iEmblemType, bPersistent);

	cairo_surface_destroy (pCairoSurface);
}

void cairo_dock_draw_emblem_from_surface (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer, CairoDockEmblem iEmblemType, gboolean bPersistent)
{
	cd_debug ("%s (%d %d)", __func__, iEmblemType, bPersistent);
	g_return_if_fail (pIcon != NULL && pContainer != NULL); 
	
	if (pSurface == NULL) 
		return;
	
	double fImgX, fImgY, emblemW = pIcon->fWidth / 3, emblemH = pIcon->fHeight / 3;
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	
	switch (iEmblemType) {
		default:
		case CAIRO_DOCK_EMBLEM_UPPER_RIGHT :
			fImgX = (pIcon->fWidth - emblemW - pIcon->fScale) * fMaxScale;
			fImgY = 1.;
		break;

		case CAIRO_DOCK_EMBLEM_LOWER_RIGHT :
			fImgX = (pIcon->fWidth - emblemW - pIcon->fScale) * fMaxScale;
			fImgY = ((pIcon->fHeight - emblemH - pIcon->fScale) * fMaxScale) + 1.;
		break;
		
		case CAIRO_DOCK_EMBLEM_UPPER_LEFT :
			fImgX = 1.;
			fImgY = 1.;
		break;
		
		case CAIRO_DOCK_EMBLEM_LOWER_LEFT :
			fImgX = 1.;
			fImgY = ((pIcon->fHeight - emblemH - pIcon->fScale) * fMaxScale) + 1.;
		break;
		
		case CAIRO_DOCK_EMBLEM_MIDDLE :
			fImgX = (pIcon->fWidth - emblemW - pIcon->fScale) * fMaxScale / 2.;
			fImgY = (pIcon->fHeight - emblemH - pIcon->fScale) * fMaxScale / 2.;
		break;
		
		case CAIRO_DOCK_EMBLEM_MIDDLE_BOTTOM:
			fImgX = (pIcon->fWidth - emblemW - pIcon->fScale) * fMaxScale / 2.;
			fImgY = ((pIcon->fHeight - emblemH - pIcon->fScale) * fMaxScale) + 1.;
		break;

		case CAIRO_DOCK_EMBLEM_BACKGROUND :
			fImgX = (pIcon->fWidth - emblemW - pIcon->fScale) * fMaxScale / 2.;
			fImgY = 0.;
			cairo_surface_t *pNewSurfaceGradated = cairo_surface_create_similar (pSurface, CAIRO_CONTENT_COLOR_ALPHA, emblemW, emblemH);
			cairo_t *pCairoContext = cairo_create (pNewSurfaceGradated);
			cairo_set_source_surface (pCairoContext, pSurface, 0, 0);

			cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0., 1., 0., (emblemH - 1.));  // de haut en bas.
			g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);

			cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
			cairo_pattern_add_color_stop_rgba (pGradationPattern, 1., 0., 0., 0., 0.);
			cairo_pattern_add_color_stop_rgba (pGradationPattern, 0., 0., 0., 0., emblemH);

			cairo_translate (pCairoContext, 0, 0);  /// superflu je pense.
			cairo_mask (pCairoContext, pGradationPattern);

			cairo_pattern_destroy (pGradationPattern);
			cairo_destroy (pCairoContext);
			pSurface = pNewSurfaceGradated;
		break;
	}
	
	//cd_debug ("Emblem: X %.0f Y %.0f W %.0f H %.0f - Icon: W %.0f H %.0f", fImgX, fImgY, emblemW, emblemH, pIcon->fWidth, pIcon->fHeight);
	
	if (!bPersistent)
		cairo_save (pIconContext);
		
	cairo_set_source_surface (pIconContext, pSurface, fImgX, fImgY);
	cairo_paint (pIconContext);
	if (iEmblemType == CAIRO_DOCK_EMBLEM_BACKGROUND)  // on a cree notre propre surface, il faut la liberer.
		cairo_surface_destroy (pSurface);
	
	if (!bPersistent)
		cairo_restore (pIconContext);
		
	cairo_dock_redraw_my_icon (pIcon, pContainer); //Test 
}

void cairo_dock_draw_emblem_classic (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer, CairoDockClassicEmblem iEmblemClassic, CairoDockEmblem iEmblemType, gboolean bPersistent)
{
	cd_debug ("%s (%s %d %d)", __func__, pIcon->acName, iEmblemClassic, iEmblemType);
	g_return_if_fail (pIcon != NULL); 
	
	gchar *cClassicEmblemPath = NULL;
	if (s_cEmblemConfPath[iEmblemClassic] == NULL) {
		switch (iEmblemClassic) {
			case CAIRO_DOCK_EMBLEM_BLANK :  // on n'affiche rien => cela effacera l'embleme.
			default :
				cClassicEmblemPath = NULL;
			break;
			case CAIRO_DOCK_EMBLEM_CHARGE:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/charge.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_DROP_INDICATOR:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/drop.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_PLAY:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/play.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_PAUSE:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/pause.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_STOP:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/stop.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_BROKEN:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/broken.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_ERROR:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/error.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_WARNING:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/warning.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
			case CAIRO_DOCK_EMBLEM_LOCKED:
				cClassicEmblemPath = g_strdup_printf ("%s/emblems/locked.svg", CAIRO_DOCK_SHARE_DATA_DIR);
			break;
		}
	}
	else
		cClassicEmblemPath = g_strdup (s_cEmblemConfPath[iEmblemClassic]);
		
	//On évite de recharger les surfaces
	double fImgX, fImgY, fImgW, fImgH, emblemW = pIcon->fWidth / 3, emblemH = pIcon->fHeight / 3;
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	if (cClassicEmblemPath == NULL)
		return ;
	if (s_pFullEmblems[iEmblemClassic].pSurface == NULL || (s_pFullEmblems[iEmblemClassic].fEmblemW != emblemW || s_pFullEmblems[iEmblemClassic].fEmblemH != emblemH) || strcmp (s_pFullEmblems[iEmblemClassic].cImagePath, cClassicEmblemPath) != 0)
	{
		if (s_pFullEmblems[iEmblemClassic].pSurface != NULL)
			cairo_surface_destroy (s_pFullEmblems[iEmblemClassic].pSurface);
		
		s_pFullEmblems[iEmblemClassic].pSurface = cairo_dock_create_surface_from_image (cClassicEmblemPath, pIconContext, fMaxScale, emblemW, emblemH, CAIRO_DOCK_KEEP_RATIO, &fImgW, &fImgH, NULL, NULL);
		s_pFullEmblems[iEmblemClassic].fEmblemW = emblemW;
		s_pFullEmblems[iEmblemClassic].fEmblemH = emblemH;
		
		if (s_pFullEmblems[iEmblemClassic].cImagePath != NULL)
			g_free (s_pFullEmblems[iEmblemClassic].cImagePath);
		s_pFullEmblems[iEmblemClassic].cImagePath = cClassicEmblemPath;
	} //On (re)charge uniquement si la surface n'existe pas, si le fichier image est différent ou si les emblemes on changés de tailles (en particulier pour les desklets)
	else
	{
		g_free (cClassicEmblemPath);
	}
	
	cairo_dock_draw_emblem_from_surface (pIconContext, s_pFullEmblems[iEmblemClassic].pSurface, pIcon, pContainer, iEmblemType, bPersistent);
}

gboolean _cairo_dock_erase_temporary_emblem (CairoDockTempEmblem *pEmblem)
{
	if (pEmblem != NULL) {
		pEmblem->iSidTimer = 0;
		cairo_dock_draw_emblem_classic (pEmblem->pIconContext, pEmblem->pIcon, pEmblem->pContainer, CAIRO_DOCK_EMBLEM_BLANK, CAIRO_DOCK_EMBLEM_MIDDLE, FALSE);
		cairo_dock_redraw_my_icon (pEmblem->pIcon, pEmblem->pContainer);
	}
	g_free (pEmblem);
	return FALSE;
}

void cairo_dock_draw_temporary_emblem_on_my_icon (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconFile, CairoDockClassicEmblem iEmblemClassic, CairoDockEmblem iEmblemType, double fTimeLength)
{
	cd_debug ("%s (%s %d %d %.0f)", __func__, cIconFile, iEmblemClassic, iEmblemType, fTimeLength);
	if (cIconFile == NULL && (iEmblemClassic < 0 || iEmblemClassic >= CAIRO_DOCK_EMBLEM_CLASSIC_NB))
		return;
	
	if (iEmblemType < 0 || iEmblemType >= CAIRO_DOCK_EMBLEM_TOTAL_NB)
		return;
	
	if (cIconFile != NULL)
		cairo_dock_draw_emblem_on_my_icon (pIconContext, cIconFile, pIcon, pContainer, iEmblemType, FALSE);
	else
		cairo_dock_draw_emblem_classic (pIconContext, pIcon, pContainer, iEmblemClassic, iEmblemType, FALSE);
	
	cairo_dock_redraw_my_icon (pIcon, pContainer);
	
	CairoDockTempEmblem *pEmblem = g_new0 (CairoDockTempEmblem, 1);
	pEmblem->pIcon = pIcon;
	pEmblem->pContainer = pContainer;
	pEmblem->pIconContext = pIconContext;
	pEmblem->iSidTimer = 0;
	
	if (fTimeLength > 0)
		pEmblem->iSidTimer = g_timeout_add (fTimeLength, (GSourceFunc) _cairo_dock_erase_temporary_emblem, (gpointer) pEmblem);
}

//A lancer a l'init du thèmes
void cairo_dock_get_emblem_path (GKeyFile *pKeyFile, gboolean *bFlushConfFileNeeded)
{
	cd_debug ("");
	g_return_if_fail (pKeyFile != NULL);
	
	g_bDisplayDropEmblem = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "show drop indicator", bFlushConfFileNeeded, TRUE, "Emblems", "drop indicator");
	
	gint i;
	GString *sKeyName = g_string_new ("");
	for (i = 1; i < CAIRO_DOCK_EMBLEM_CLASSIC_NB; i++) {
		g_string_printf (sKeyName, "emblem_%d", i);
		s_cEmblemConfPath[i] = cairo_dock_get_string_key_value (pKeyFile, "Indicators", sKeyName->str, bFlushConfFileNeeded, NULL, "Emblems", NULL);
	}
	g_string_free (sKeyName, TRUE);
}

//A lancer a la sortie du dock
void cairo_dock_free_emblem (void)
{
	gint i;
	
	for (i = 1; i < CAIRO_DOCK_EMBLEM_CLASSIC_NB; i++) {
		g_free (s_cEmblemConfPath[i]);
		s_cEmblemConfPath[i] = NULL;
	}
	for (i = 0; i < CAIRO_DOCK_EMBLEM_CLASSIC_NB; i++) {
		if (s_pFullEmblems[i].pSurface != NULL) {
			cairo_surface_destroy (s_pFullEmblems[i].pSurface);
			s_pFullEmblems[i].pSurface = NULL;
		}
		if (s_pFullEmblems[i].cImagePath != NULL) {
			g_free (s_pFullEmblems[i].cImagePath);
			s_pFullEmblems[i].cImagePath = NULL;
		}
	}
}

//A lancer quand la configuration est mise a jour
void cairo_dock_updated_emblem_conf_file (GKeyFile *pKeyFile, gboolean *bFlushConfFileNeeded)
{
	cairo_dock_free_emblem ();
	cairo_dock_get_emblem_path (pKeyFile, bFlushConfFileNeeded);
}
