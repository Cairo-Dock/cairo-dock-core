/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-config.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-applet-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applet-facility.h"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCairoDockDataDir;

extern gboolean g_bUseOpenGL;


void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (cairo_status (pIconContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface l'ancienne image.
	cairo_set_source_rgba (pIconContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pIconContext);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_OVER);
	
	//\________________ On applique la nouvelle image.
	if (pSurface != NULL && fScale > 0)
	{
		cairo_save (pIconContext);
		if (fScale != 1 && pIcon != NULL)
		{
			double fMaxScale = cairo_dock_get_max_scale (pContainer);
			cairo_translate (pIconContext, pIcon->fWidth * fMaxScale / 2 * (1 - fScale) , pIcon->fHeight * fMaxScale / 2 * (1 - fScale));
			cairo_scale (pIconContext, fScale, fScale);
		}
		
		cairo_set_source_surface (
			pIconContext,
			pSurface,
			0.,
			0.);
		
		if (fAlpha != 1)
			cairo_paint_with_alpha (pIconContext, fAlpha);
		else
			cairo_paint (pIconContext);
		cairo_restore (pIconContext);
	}
	if (g_bUseOpenGL)
		cairo_dock_update_icon_texture (pIcon);
}

void cairo_dock_add_reflection_to_icon (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon != NULL && pContainer!= NULL);
	if (pIcon->pReflectionBuffer != NULL)
		cairo_surface_destroy (pIcon->pReflectionBuffer);
	
	double fMaxScale = (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) / CAIRO_DOCK (pContainer)->fRatio : 1);
	gboolean bIsHorizontal = pContainer->bIsHorizontal;
	pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
		pIconContext,
		(bIsHorizontal ? pIcon->fWidth : pIcon->fHeight) * fMaxScale,
		(bIsHorizontal ? pIcon->fHeight : pIcon->fWidth) * fMaxScale,
		bIsHorizontal,
		fMaxScale,
		pContainer->bDirectionUp);
}

void cairo_dock_set_icon_surface_with_reflect (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer)
{
	cairo_dock_set_icon_surface_full (pIconContext, pSurface, 1., 1., pIcon, pContainer);
	
	cairo_dock_add_reflection_to_icon (pIconContext, pIcon, pContainer);
}

void cairo_dock_set_image_on_icon (cairo_t *pIconContext, gchar *cImagePath, Icon *pIcon, CairoContainer *pContainer)
{
	double fMaxScale = (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) / CAIRO_DOCK (pContainer)->fRatio : 1);
	cairo_surface_t *pImageSurface = cairo_dock_create_surface_for_icon (cImagePath,
		pIconContext,
		pIcon->fWidth * fMaxScale,
		pIcon->fHeight * fMaxScale);
	
	cairo_dock_set_icon_surface_with_reflect (pIconContext, pImageSurface, pIcon, pContainer);
	
	cairo_surface_destroy (pImageSurface);
}

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (cairo_status (pIconContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface l'ancienne image.
	cairo_set_source_rgba (pIconContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pIconContext);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_OVER);
	
	//\________________ On applique la nouvelle image.
	if (pSurface != NULL)
	{
		cairo_set_source_surface (
			pIconContext,
			pSurface,
			0.,
			0.);
		cairo_paint (pIconContext);
	}
	
	//\________________ On dessine la barre.
	cairo_dock_draw_bar_on_icon (pIconContext, fValue, pIcon, pContainer);
	
	if (g_bUseOpenGL)
		cairo_dock_update_icon_texture (pIcon);
}

void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon, CairoContainer *pContainer)
{
	double fMaxScale = (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) / CAIRO_DOCK (pContainer)->fRatio : 1);
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (0.,
		0.,
		pIcon->fWidth * fMaxScale,
		0.);  // de gauche a droite.
	g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
	
	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		1.,
		0.,
		0.,
		1.);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		0.,
		1.,
		0.,
		1.);
	
	cairo_save (pIconContext);
	cairo_set_operator (pIconContext, CAIRO_OPERATOR_OVER);
	
	cairo_set_line_width (pIconContext, 6);
	cairo_set_line_cap (pIconContext, CAIRO_LINE_CAP_ROUND);
	
	cairo_move_to (pIconContext, 3, pIcon->fHeight * fMaxScale - 3);
	cairo_rel_line_to (pIconContext, (pIcon->fWidth * fMaxScale - 6) * fValue, 0);
	
	cairo_set_source (pIconContext, pGradationPattern);
	cairo_stroke (pIconContext);
	
	cairo_pattern_destroy (pGradationPattern);
	cairo_restore (pIconContext);
}

void cairo_dock_set_hours_minutes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int hours = iTimeInSeconds / 3600;
	int minutes = (iTimeInSeconds % 3600) / 60;
	if (hours != 0)
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dh%02d", hours, abs (minutes));
	else
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dmn", minutes);
}

void cairo_dock_set_minutes_secondes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds)
{
	int minutes = iTimeInSeconds / 60;
	int secondes = iTimeInSeconds % 60;
	if (minutes != 0)
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%d:%02d", minutes, abs (secondes));
	else
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%s0:%02d", (secondes < 0 ? "-" : ""), abs (secondes));
}

void cairo_dock_set_size_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes)
{
	if (iSizeInBytes < 1024)
	{
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dB", iSizeInBytes);
	}
	else if (iSizeInBytes < (1 << 20))
	{
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dK", (int) (iSizeInBytes>>10));
	}
	else if (iSizeInBytes < (1 << 30))
	{
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dM", (int) (iSizeInBytes>>20));
	}
	else
	{
		cairo_dock_set_quick_info_full (pSourceContext, pIcon, pContainer, "%dG", (int) (iSizeInBytes>>30));
	}
}

gchar *cairo_dock_get_theme_path_for_module (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName)
{
	gchar *cThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	
	gchar *cUserThemesDir = (cExtraDirName != NULL ? g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, cExtraDirName) : NULL);
	gchar *cThemePath = cairo_dock_get_theme_path (cThemeName, cShareThemesDir, cUserThemesDir, cExtraDirName);
	
	g_free (cThemeName);
	g_free (cUserThemesDir);
	return cThemePath;
}


GtkWidget *cairo_dock_create_sub_menu (gchar *cLabel, GtkWidget *pMenu)
{
	GtkWidget *pSubMenu = gtk_menu_new ();
	GtkWidget *pMenuItem = gtk_menu_item_new_with_label (cLabel);
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);
	return pSubMenu;
}



static gboolean _cairo_dock_timer (CairoDockMeasure *pMeasureTimer)
{
	cairo_dock_launch_measure (pMeasureTimer);
	return TRUE;
}
static gpointer _cairo_dock_threaded_calculation (CairoDockMeasure *pMeasureTimer)
{
	//cd_debug ("*** debut du thread (%d)", g_atomic_int_get (&pMeasureTimer->iThreadIsRunning));
	if (pMeasureTimer->acquisition != NULL)
		pMeasureTimer->acquisition (pMeasureTimer->pUserData);
	
	g_mutex_lock (pMeasureTimer->pMutexData);
	pMeasureTimer->read (pMeasureTimer->pUserData);
	g_mutex_unlock (pMeasureTimer->pMutexData);
	
	g_atomic_int_set (&pMeasureTimer->iThreadIsRunning, 0);
	return NULL;
}
static gboolean _cairo_dock_check_for_redraw (CairoDockMeasure *pMeasureTimer)
{
	int iThreadIsRunning = g_atomic_int_get (&pMeasureTimer->iThreadIsRunning);
	//cd_debug ("%s (%d)", __func__, iThreadIsRunning);
	if (! iThreadIsRunning)
	{
		//\_______________________ On recharge ce qu'il faut avec ces nouvelles donnees.
		g_mutex_lock (pMeasureTimer->pMutexData);
		gboolean bContinue = pMeasureTimer->update (pMeasureTimer->pUserData);
		g_mutex_unlock (pMeasureTimer->pMutexData);
		
		//\_______________________ On lance/arrete le timer si necessaire.
		if (! bContinue)
		{
			if (pMeasureTimer->iSidTimer != 0)
			{
				g_source_remove (pMeasureTimer->iSidTimer);
				pMeasureTimer->iSidTimer = 0;
			}
		}
		else if (pMeasureTimer->iSidTimer == 0 && pMeasureTimer->iCheckInterval != 0)
		{
			pMeasureTimer->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
			pMeasureTimer->iSidTimer = g_timeout_add_seconds (pMeasureTimer->iCheckInterval, (GSourceFunc) _cairo_dock_timer, pMeasureTimer);
		}
		
		pMeasureTimer->iSidTimerRedraw = 0;
		return FALSE;
	}
	return TRUE;
}
void cairo_dock_launch_measure (CairoDockMeasure *pMeasureTimer)
{
	g_return_if_fail (pMeasureTimer != NULL);
	if (pMeasureTimer->pMutexData == NULL)
	{
		if (pMeasureTimer->acquisition != NULL)
			pMeasureTimer->acquisition (pMeasureTimer->pUserData);
		if (pMeasureTimer->read != NULL)
			pMeasureTimer->read (pMeasureTimer->pUserData);
		gboolean bContinue = pMeasureTimer->update (pMeasureTimer->pUserData);
		
		if (! bContinue)
		{
			if (pMeasureTimer->iSidTimer != 0)
			{
				g_source_remove (pMeasureTimer->iSidTimer);
				pMeasureTimer->iSidTimer = 0;
			}
		}
		else if (pMeasureTimer->iSidTimer == 0 && pMeasureTimer->iCheckInterval != 0)
		{
			pMeasureTimer->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
			pMeasureTimer->iSidTimer = g_timeout_add_seconds (pMeasureTimer->iCheckInterval, (GSourceFunc) _cairo_dock_timer, pMeasureTimer);
		}
	}
	else if (g_atomic_int_compare_and_exchange (&pMeasureTimer->iThreadIsRunning, 0, 1))  // il etait egal a 0, on lui met 1 et on lance le thread.
	{
		//cd_debug (" ==> lancement du thread de calcul");
		
		if (pMeasureTimer->iSidTimerRedraw == 0) 
			pMeasureTimer->iSidTimerRedraw = g_timeout_add (MAX (150, MIN (0.15 * pMeasureTimer->iCheckInterval, 333)), (GSourceFunc) _cairo_dock_check_for_redraw, pMeasureTimer);
		
		GError *erreur = NULL;
		GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_threaded_calculation, pMeasureTimer, FALSE, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
		}
	}
	else if (pMeasureTimer->iSidTimer == 0 && pMeasureTimer->iCheckInterval != 0)
	{
		pMeasureTimer->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
		pMeasureTimer->iSidTimer = g_timeout_add_seconds (pMeasureTimer->iCheckInterval, (GSourceFunc) _cairo_dock_timer, pMeasureTimer);
	}
}

static gboolean _cairo_dock_one_shot_timer (CairoDockMeasure *pMeasureTimer)
{
	pMeasureTimer->iSidTimerRedraw = 0;
	cairo_dock_launch_measure (pMeasureTimer);
	return FALSE;
}
void cairo_dock_launch_measure_delayed (CairoDockMeasure *pMeasureTimer, double fDelay)
{
	pMeasureTimer->iSidTimerRedraw = g_timeout_add (fDelay, (GSourceFunc) _cairo_dock_one_shot_timer, pMeasureTimer);
}

CairoDockMeasure *cairo_dock_new_measure_timer (int iCheckInterval, CairoDockAquisitionTimerFunc acquisition, CairoDockReadTimerFunc read, CairoDockUpdateTimerFunc update, gpointer pUserData)
{
	CairoDockMeasure *pMeasureTimer = g_new0 (CairoDockMeasure, 1);
	if (read != NULL || acquisition != NULL)
		pMeasureTimer->pMutexData = g_mutex_new ();
	pMeasureTimer->iCheckInterval = iCheckInterval;
	pMeasureTimer->acquisition = acquisition;
	pMeasureTimer->read = read;
	pMeasureTimer->update = update;
	pMeasureTimer->pUserData = pUserData;
	return pMeasureTimer;
}

static void _cairo_dock_pause_measure_timer (CairoDockMeasure *pMeasureTimer)
{
	if (pMeasureTimer == NULL)
		return ;
	if (pMeasureTimer->iSidTimerRedraw != 0)
	{
		g_source_remove (pMeasureTimer->iSidTimerRedraw);
		pMeasureTimer->iSidTimerRedraw = 0;
	}
	if (pMeasureTimer->iSidTimer!= 0)
	{
		g_source_remove (pMeasureTimer->iSidTimer);
		pMeasureTimer->iSidTimer= 0;
	}
}

void cairo_dock_stop_measure_timer (CairoDockMeasure *pMeasureTimer)
{
	if (pMeasureTimer == NULL)
		return ;
	
	_cairo_dock_pause_measure_timer (pMeasureTimer);
	
	cd_message ("***on attend que le thread termine...(%d)", g_atomic_int_get (&pMeasureTimer->iThreadIsRunning));
	while (g_atomic_int_get (&pMeasureTimer->iThreadIsRunning))
		g_usleep (10);
		///gtk_main_iteration ();
	cd_message ("***temine.");
}

void cairo_dock_free_measure_timer (CairoDockMeasure *pMeasureTimer)
{
	if (pMeasureTimer == NULL)
		return ;
	cairo_dock_stop_measure_timer (pMeasureTimer);
	
	if (pMeasureTimer->pMutexData != NULL)
		g_mutex_free (pMeasureTimer->pMutexData);
	g_free (pMeasureTimer);
}

gboolean cairo_dock_measure_is_active (CairoDockMeasure *pMeasureTimer)
{
	return (pMeasureTimer != NULL && pMeasureTimer->iSidTimer != 0);
}

static void _cairo_dock_restart_timer_with_frequency (CairoDockMeasure *pMeasureTimer, int iNewCheckInterval)
{
	gboolean bNeedsRestart = (pMeasureTimer->iSidTimer != 0);
	_cairo_dock_pause_measure_timer (pMeasureTimer);
	
	if (bNeedsRestart && iNewCheckInterval != 0)
		pMeasureTimer->iSidTimer = g_timeout_add_seconds (iNewCheckInterval, (GSourceFunc) _cairo_dock_timer, pMeasureTimer);
}

void cairo_dock_change_measure_frequency (CairoDockMeasure *pMeasureTimer, int iNewCheckInterval)
{
	g_return_if_fail (pMeasureTimer != NULL);
	pMeasureTimer->iCheckInterval = iNewCheckInterval;
	
	_cairo_dock_restart_timer_with_frequency (pMeasureTimer, iNewCheckInterval);
}

void cairo_dock_relaunch_measure_immediately (CairoDockMeasure *pMeasureTimer, int iNewCheckInterval)
{
	cairo_dock_stop_measure_timer (pMeasureTimer);  // on stoppe avant car on ne veut pas attendre la prochaine iteration.
	cairo_dock_change_measure_frequency (pMeasureTimer, iNewCheckInterval); // nouvelle frequence eventuelement.
	cairo_dock_launch_measure (pMeasureTimer);  // mesure immediate.
}

void cairo_dock_downgrade_frequency_state (CairoDockMeasure *pMeasureTimer)
{
	if (pMeasureTimer->iFrequencyState < CAIRO_DOCK_FREQUENCY_SLEEP)
	{
		pMeasureTimer->iFrequencyState ++;
		int iNewCheckInterval;
		switch (pMeasureTimer->iFrequencyState)
		{
			case CAIRO_DOCK_FREQUENCY_LOW :
				iNewCheckInterval = 2 * pMeasureTimer->iCheckInterval;
			break ;
			case CAIRO_DOCK_FREQUENCY_VERY_LOW :
				iNewCheckInterval = 4 * pMeasureTimer->iCheckInterval;
			break ;
			case CAIRO_DOCK_FREQUENCY_SLEEP :
				iNewCheckInterval = 10 * pMeasureTimer->iCheckInterval;
			break ;
			default :  // ne doit pas arriver.
				iNewCheckInterval = pMeasureTimer->iCheckInterval;
			break ;
		}
		
		cd_message ("degradation de la mesure (etat <- %d/%d)", pMeasureTimer->iFrequencyState, CAIRO_DOCK_NB_FREQUENCIES-1);
		_cairo_dock_restart_timer_with_frequency (pMeasureTimer, iNewCheckInterval);
	}
}

void cairo_dock_set_normal_frequency_state (CairoDockMeasure *pMeasureTimer)
{
	if (pMeasureTimer->iFrequencyState != CAIRO_DOCK_FREQUENCY_NORMAL)
	{
		pMeasureTimer->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
		_cairo_dock_restart_timer_with_frequency (pMeasureTimer, pMeasureTimer->iCheckInterval);
	}
}

//Utile pour jouer des fichiers son depuis le dock.
//A utiliser avec l'Objet UI 'u' dans les .conf
void cairo_dock_play_sound (const gchar *cSoundPath)
{
	cd_debug ("%s (%s)", __func__, cSoundPath);
	if (cSoundPath == NULL)
	{
		cd_warning ("No sound to play, halt.");
		return;
	}
	
	GError *erreur = NULL;
	gchar *cSoundCommand = NULL;
	if (g_file_test ("/usr/bin/play", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("play \"%s\"", cSoundPath);
		
	else if (g_file_test ("/usr/bin/aplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("aplay \"%s\"", cSoundPath);
	
	else if (g_file_test ("/usr/bin/paplay", G_FILE_TEST_EXISTS))
		cSoundCommand = g_strdup_printf("paplay \"%s\"", cSoundPath);
	
	cairo_dock_launch_command (cSoundCommand);
	
	g_free (cSoundCommand);
}

void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro) {
	gchar *cContent = NULL;
	gsize length = 0;
	GError *erreur = NULL;
	g_file_get_contents ("/usr/share/gnome-about/gnome-version.xml", &cContent, &length, &erreur);
	
	if (erreur != NULL) {
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		*iMajor = 0;
		*iMinor = 0;
		*iMicro = 0;
		return;
	}
	
	gchar **cLineList = g_strsplit (cContent, "\n", -1);
	gchar *cOneLine = NULL, *cMajor = NULL, *cMinor = NULL, *cMicro = NULL;
	int i, iMaj = 0, iMin = 0, iMic = 0;
	for (i = 0; cLineList[i] != NULL; i ++) {
		cOneLine = cLineList[i];
		if (*cOneLine == '\0')
			continue;
		
		//Seeking for Major
		cMajor = g_strstr_len (cOneLine, -1, "<platform>");  //<platform>2</platform>
		if (cMajor != NULL) {
			cMajor += 10; //On saute <platform>
			gchar *str = strchr (cMajor, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </platform>
			iMaj = atoi (cMajor);
		}
		else { //Gutsy xml's doesn't have <platform> but <major>
			cMajor = g_strstr_len (cOneLine, -1, "<major>");  //<major>2</major>
			if (cMajor != NULL) {
				cMajor += 7; //On saute <major>
				gchar *str = strchr (cMajor, '<');
				if (str != NULL)
					*str = '\0'; //On bloque a </major>
				iMaj = atoi (cMajor);
			}
		}
		
		//Seeking for Minor
		cMinor = g_strstr_len (cOneLine, -1, "<minor>");  //<minor>22</minor>
		if (cMinor != NULL) {
			cMinor += 7; //On saute <minor>
			gchar *str = strchr (cMinor, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </minor>
			iMin = atoi (cMinor);
		}
		
		//Seeking for Micro
		cMicro = g_strstr_len (cOneLine, -1, "<micro>");  //<micro>3</micro>
		if (cMicro != NULL) {
			cMicro += 7; //On saute <micro>
			gchar *str = strchr (cMicro, '<');
			if (str != NULL)
				*str = '\0'; //On bloque a </micro>
			iMic = atoi (cMicro);
		}
		
		if (iMaj != 0 && iMin != 0 && iMic != 0)
			break; //On s'enfou du reste
	}
	
	cd_debug ("Gnome Version %d.%d.%d", iMaj, iMin, iMic);
	
	*iMajor = iMaj;
	*iMinor = iMin;
	*iMicro = iMic;
	
	g_free (cContent);
	g_strfreev (cLineList);
}
