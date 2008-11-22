/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-config.h"

#define CAIRO_DOCK_TYPE_CONF_FILE_FILE ".cairo-dock-conf-file"

static const gchar * s_cIconTypeNames[(CAIRO_DOCK_NB_TYPES+1)/2] = {"launchers", "applications", "applets"};

extern CairoDock *g_pMainDock;
extern gchar *g_cMainDockDefaultRendererName;
extern gchar *g_cSubDockDefaultRendererName;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCairoDockDataDir;
extern gchar *g_cCurrentLaunchersPath;

extern int g_iSinusoidWidth;
extern double g_fAmplitude;
extern int g_iIconGap;
extern double g_fReflectSize;
extern double g_fAlbedo;
extern double g_fAlphaAtRest;

extern gboolean g_bSameHorizontality;
extern double g_fSubDockSizeRatio;

extern gpointer *g_pDefaultIconDirectory;
static gboolean s_bUserTheme = FALSE;
extern gchar *g_cCurrentLaunchersPath;
extern gchar *g_cConfFile;

extern double g_fBackgroundImageWidth;
extern double g_fBackgroundImageHeight;

extern int g_iDockRadius;
extern gint g_iFrameMargin;
extern int g_iDockLineWidth;
extern gboolean g_bRoundedBottomCorner;
extern double g_fLineColor[4];
extern gint g_iStringLineWidth;
extern double g_fStringColor[4];

extern gboolean g_bBackgroundImageRepeat;
extern double g_fBackgroundImageAlpha;
extern gchar *g_cBackgroundImageFile;

extern double g_fStripesColorBright[4];
extern double g_fStripesColorDark[4];
extern cairo_surface_t *g_pStripesBuffer;
extern int g_iNbStripes;
extern double g_fStripesWidth;
extern double g_fStripesAngle;

extern int g_iScreenWidth[2];
extern int g_iScreenHeight[2];

extern int g_tIconAuthorizedWidth[CAIRO_DOCK_NB_TYPES];
extern int g_tIconAuthorizedHeight[CAIRO_DOCK_NB_TYPES];
extern int g_tAnimationType[CAIRO_DOCK_NB_TYPES];
extern int g_tNbAnimationRounds[CAIRO_DOCK_NB_TYPES];
extern int g_tIconTypeOrder[CAIRO_DOCK_NB_TYPES];

extern gboolean g_bUseSeparator;
extern gchar *g_cSeparatorImage;
extern gboolean g_bRevolveSeparator;
extern gboolean g_bConstantSeparatorSize;

extern CairoDockLabelDescription g_iconTextDescription;
extern CairoDockLabelDescription g_quickInfoTextDescription;
extern CairoDockLabelDescription g_dialogTextDescription;

extern cairo_surface_t *g_pDesktopBgSurface;

extern gchar *g_cDeskletDecorationsName;
extern int g_iDeskletButtonSize;
extern gchar *g_cRotateButtonImage;
extern gchar *g_cRetachButtonImage;

static gchar **g_cUseXIconAppliList = NULL;
static gboolean s_bLoading = FALSE;


gboolean cairo_dock_get_boolean_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gboolean bDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gboolean bValue = g_key_file_get_boolean (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		bValue = g_key_file_get_boolean (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		g_free (cGroupNameUpperCase);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			bValue = g_key_file_get_boolean (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				bValue = g_key_file_get_boolean (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					bValue = bDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}

		g_key_file_set_boolean (pKeyFile, cGroupName, cKeyName, bValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return bValue;
}

int cairo_dock_get_integer_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, int iDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	int iValue = g_key_file_get_integer (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		iValue = g_key_file_get_integer (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			iValue = g_key_file_get_integer (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				iValue = g_key_file_get_integer (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					iValue = iDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_integer (pKeyFile, cGroupName, cKeyName, iValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return iValue;
}

double cairo_dock_get_double_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, double fDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	double fValue = g_key_file_get_double (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		fValue = g_key_file_get_double (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			fValue = g_key_file_get_double (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				fValue = g_key_file_get_double (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					fValue = fDefaultValue;
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_double (pKeyFile, cGroupName, cKeyName, fValue);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	return fValue;
}

gchar *cairo_dock_get_string_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gchar *cValue = g_key_file_get_string (pKeyFile, cGroupName, cKeyName, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		cValue = g_key_file_get_string (pKeyFile, cGroupNameUpperCase, cKeyName, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			cValue = g_key_file_get_string (pKeyFile, "Cairo Dock", cKeyName, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				cValue = g_key_file_get_string (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
					cValue = g_strdup (cDefaultValue);
				}
				else
					cd_message (" (recuperee)");
			}
			else
				cd_message (" (recuperee)");
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, (cValue != NULL ? cValue : ""));
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	if (cValue != NULL && *cValue == '\0')
	{
		g_free (cValue);
		cValue = NULL;
	}
	return cValue;
}

void cairo_dock_get_integer_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, int *iValueBuffer, int iNbElements, int *iDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gsize length = 0;
	if (iDefaultValues != NULL)
		memcpy (iValueBuffer, iDefaultValues, iNbElements * sizeof (int));

	int *iValuesList = g_key_file_get_integer_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		iValuesList = g_key_file_get_integer_list (pKeyFile, cGroupNameUpperCase, cKeyName, &length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			iValuesList = g_key_file_get_integer_list (pKeyFile, "Cairo Dock", cKeyName, &length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				iValuesList = g_key_file_get_integer_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &length, &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
				}
				else
				{
					cd_message (" (recuperee)");
					if (length > 0)
						memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
				}
			}
			else
			{
				cd_message (" (recuperee)");
				if (length > 0)
					memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
			}
		}
		else
		{
			if (length > 0)
				memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_integer_list (pKeyFile, cGroupName, cKeyName, iValueBuffer, iNbElements);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	else
	{
		if (length > 0)
			memcpy (iValueBuffer, iValuesList, MIN (iNbElements, length) * sizeof (int));
	}
	g_free (iValuesList);
}

void cairo_dock_get_double_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, double *fValueBuffer, int iNbElements, double *fDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	gsize length = 0;
	if (fDefaultValues != NULL)
		memcpy (fValueBuffer, fDefaultValues, iNbElements * sizeof (double));

	double *fValuesList = g_key_file_get_double_list (pKeyFile, cGroupName, cKeyName, &length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		fValuesList = g_key_file_get_double_list (pKeyFile, cGroupNameUpperCase, cKeyName, &length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			fValuesList = g_key_file_get_double_list (pKeyFile, "Cairo Dock", cKeyName, &length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				erreur = NULL;
				fValuesList = g_key_file_get_double_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), &length, &erreur);
				if (erreur != NULL)
				{
					g_error_free (erreur);
				}
				else
				{
					cd_message (" (recuperee)");
					if (length > 0)
						memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
				}
			}
			else
			{
				cd_message (" (recuperee)");
				if (length > 0)
					memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
			}
		}
		else
		{
			if (length > 0)
				memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
		}
		g_free (cGroupNameUpperCase);

		g_key_file_set_double_list (pKeyFile, cGroupName, cKeyName, fValueBuffer, iNbElements);
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	else
	{
		if (length > 0)
			memcpy (fValueBuffer, fValuesList, MIN (iNbElements, length) * sizeof (double));
	}
	g_free (fValuesList);
}

gchar **cairo_dock_get_string_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gsize *length, gchar *cDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	GError *erreur = NULL;
	*length = 0;
	gchar **cValuesList = g_key_file_get_string_list (pKeyFile, cGroupName, cKeyName, length, &erreur);
	if (erreur != NULL)
	{
		if (bFlushConfFileNeeded != NULL)
			cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;

		gchar* cGroupNameUpperCase = g_ascii_strup (cGroupName, -1);
		cValuesList = g_key_file_get_string_list (pKeyFile, cGroupNameUpperCase, cKeyName, length, &erreur);
		if (erreur != NULL)
		{
			g_error_free (erreur);
			erreur = NULL;
			cValuesList = g_key_file_get_string_list (pKeyFile, (cDefaultGroupName != NULL ? cDefaultGroupName : cGroupName), (cDefaultKeyName != NULL ? cDefaultKeyName : cKeyName), length, &erreur);
			if (erreur != NULL)
			{
				g_error_free (erreur);
				cValuesList = g_strsplit (cDefaultValues, ";", -1);  // "" -> NULL.
				int i = 0;
				if (cValuesList != NULL)
				{
					while (cValuesList[i] != NULL)
						i ++;
				}
				*length = i;
			}
		}
		g_free (cGroupNameUpperCase);

		if (*length > 0)
			g_key_file_set_string_list (pKeyFile, cGroupName, cKeyName, (const gchar **)cValuesList, *length);
		else
			g_key_file_set_string (pKeyFile, cGroupName, cKeyName, "");
		if (bFlushConfFileNeeded != NULL)
			*bFlushConfFileNeeded = TRUE;
	}
	if (cValuesList != NULL && (cValuesList[0] == NULL || (*(cValuesList[0]) == '\0' && *length == 1)))
	{
		g_strfreev (cValuesList);
		cValuesList = NULL;
		*length = 0;
	}
	return cValuesList;
}

CairoDockAnimationType cairo_dock_get_animation_type_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, CairoDockAnimationType iDefaultAnimation, gchar *cDefaultGroupName, gchar *cDefaultKeyName)
{
	CairoDockAnimationType iAnimationType = cairo_dock_get_integer_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, iDefaultAnimation, cDefaultGroupName, cDefaultKeyName);
	if (iAnimationType < 0 || iAnimationType >= CAIRO_DOCK_NB_ANIMATIONS)
		iAnimationType = 0;
	return iAnimationType;
}

gchar *cairo_dock_get_file_path_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultGroupName, gchar *cDefaultKeyName, gchar *cDefaultDir, gchar *cDefaultFileName)
{
	gchar *cFileName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, NULL, cDefaultGroupName, cDefaultKeyName);
	gchar *cFilePath = NULL;
	if (cFileName != NULL)
		cFilePath = cairo_dock_generate_file_path (cFileName);
	else if (cDefaultFileName != NULL && cDefaultDir != NULL)
		cFilePath = g_strdup_printf ("%s/%s", cDefaultDir, cDefaultFileName);
	return cFilePath;
}


#define GET_GROUP_CONFIG_BEGIN(cGroupName) \
static gboolean cairo_dock_read_conf_file_##cGroupName (GKeyFile *pKeyFile, CairoDock *pDock) \
{ \
	gboolean bFlushConfFileNeeded = FALSE;
	
#define GET_GROUP_CONFIG_END \
	return bFlushConfFileNeeded; \
}

#define cairo_dock_read_group_conf_file(cGroupName) \
cairo_dock_read_conf_file_##cGroupName (pKeyFile, pDock)

/*static CairoDockPositionType s_iScreenBorder=0;
GET_GROUP_CONFIG_BEGIN (Position)
	pDock->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Position", "x gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	pDock->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Position", "y gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	s_iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Position", "screen border", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (s_iScreenBorder < 0 || s_iScreenBorder >= CAIRO_DOCK_NB_POSITIONS)
		s_iScreenBorder = 0;

	pDock->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Position", "alignment", &bFlushConfFileNeeded, 0.5, NULL, NULL);
GET_GROUP_CONFIG_END


GET_GROUP_CONFIG_BEGIN (Accessibility)
	g_bReserveSpace = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "reserve space", &bFlushConfFileNeeded, FALSE, "Position", NULL);

	cairo_dock_deactivate_temporary_auto_hide ();
	pDock->bAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "auto-hide", &bFlushConfFileNeeded, FALSE, "Position", "auto-hide");
	
	g_bPopUp = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop-up", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	g_bPopUpOnScreenBorder = ! cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "pop in corner only", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	
	g_iMaxAuthorizedWidth = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "max autorized width", &bFlushConfFileNeeded, 0, "Position", NULL);
	
	if (g_cRaiseDockShortcut != NULL)
	{
		cd_keybinder_unbind (g_cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
		g_free (g_cRaiseDockShortcut);
	}
	g_cRaiseDockShortcut = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	g_iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	g_iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
GET_GROUP_CONFIG_END*/

/*static gchar **s_cActiveModuleList = NULL;
GET_GROUP_CONFIG_BEGIN (System)
	g_bUseFakeTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "fake transparency", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	g_bLabelForPointedIconOnly = cairo_dock_get_boolean_key_value (pKeyFile, "System", "pointed icon only", &bFlushConfFileNeeded, FALSE, "Labels", NULL);
	g_fLabelAlphaThreshold = cairo_dock_get_double_key_value (pKeyFile, "System", "alpha threshold", &bFlushConfFileNeeded, 10., "Labels", NULL);
	g_bTextAlwaysHorizontal = cairo_dock_get_boolean_key_value (pKeyFile, "System", "always horizontal", &bFlushConfFileNeeded, FALSE, "Labels", NULL);
	
	double fUserValue = cairo_dock_get_double_key_value (pKeyFile, "System", "unfold factor", &bFlushConfFileNeeded, 8., "Cairo Dock", NULL);
	g_fUnfoldAcceleration = 1 - pow (2, - fUserValue);
	g_bAnimateOnAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate on auto-hide", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	int iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "grow up steps", &bFlushConfFileNeeded, 8, "Cairo Dock", NULL);
	iNbSteps = MAX (iNbSteps, 1);
	g_iGrowUpInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	iNbSteps = cairo_dock_get_integer_key_value (pKeyFile, "System", "shrink down steps", &bFlushConfFileNeeded, 10, "Cairo Dock", NULL);
	iNbSteps = MAX (iNbSteps, 1);
	g_iShrinkDownInterval = MAX (1, CAIRO_DOCK_NB_MAX_ITERATIONS / iNbSteps);
	g_fMoveUpSpeed = cairo_dock_get_double_key_value (pKeyFile, "System", "move up speed", &bFlushConfFileNeeded, 0.35, "Cairo Dock", NULL);
	g_fMoveUpSpeed = MAX (0.01, MIN (g_fMoveUpSpeed, 1));
	g_fMoveDownSpeed = cairo_dock_get_double_key_value (pKeyFile, "System", "move down speed", &bFlushConfFileNeeded, 0.35, "Cairo Dock", NULL);
	g_fMoveDownSpeed = MAX (0.01, MIN (g_fMoveDownSpeed, 1));
	
	int iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "refresh frequency", &bFlushConfFileNeeded, 25, "Cairo Dock", NULL);
	g_fRefreshInterval = 1000. / iRefreshFrequency;
	g_bDynamicReflection = cairo_dock_get_boolean_key_value (pKeyFile, "System", "dynamic reflection", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	g_bAnimateSubDock = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate subdocks", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);
	
	g_fStripesSpeedFactor = cairo_dock_get_double_key_value (pKeyFile, "System", "scroll speed factor", &bFlushConfFileNeeded, 1.0, "Background", NULL);
	g_fStripesSpeedFactor = MIN (1., g_fStripesSpeedFactor);
	g_bDecorationsFollowMouse = cairo_dock_get_boolean_key_value (pKeyFile, "System", "decorations enslaved", &bFlushConfFileNeeded, TRUE, "Background", NULL);
	
	g_iScrollAmount = cairo_dock_get_integer_key_value (pKeyFile, "System", "scroll amount", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	g_bResetScrollOnLeave = cairo_dock_get_boolean_key_value (pKeyFile, "System", "reset scroll", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	g_fScrollAcceleration = cairo_dock_get_double_key_value (pKeyFile, "System", "reset scroll acceleration", &bFlushConfFileNeeded, 0.9, "Icons", NULL);
	
	gsize length=0;
	s_cActiveModuleList = cairo_dock_get_string_list_key_value (pKeyFile, "System", "modules", &bFlushConfFileNeeded, &length, NULL, "Applets", "modules_0");
	
	g_iFileSortType = cairo_dock_get_integer_key_value (pKeyFile, "System", "sort files", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
	g_bShowHiddenFiles = cairo_dock_get_boolean_key_value (pKeyFile, "System", "show hidden files", &bFlushConfFileNeeded, FALSE, NULL, NULL);
GET_GROUP_CONFIG_END*/


/*GET_GROUP_CONFIG_BEGIN (TaskBar)
	g_bShowAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "show applications", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	g_bUniquePid = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "unique PID", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	g_bGroupAppliByClass = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "group by class", &bFlushConfFileNeeded, FALSE, "Applications", NULL);

	g_iAppliMaxNameLength = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "max name length", &bFlushConfFileNeeded, 15, "Applications", NULL);

	g_bMinimizeOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "minimize on click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	g_bCloseAppliOnMiddleClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "close on middle click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);

	g_bAutoHideOnFullScreen = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "auto quick hide", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	g_bAutoHideOnMaximized = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "auto quick hide on max", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	g_bHideVisibleApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "hide visible", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	g_fVisibleAppliAlpha = cairo_dock_get_double_key_value (pKeyFile, "TaskBar", "visibility alpha", &bFlushConfFileNeeded, .25, "Applications", NULL);  // >0 <=> les fenetres minimisees sont transparentes.
	if (g_bHideVisibleApplis && g_fVisibleAppliAlpha < 0)
		g_fVisibleAppliAlpha = 0.;  // on inhibe ce parametre, puisqu'il ne sert alors a rien.
	g_bAppliOnCurrentDesktopOnly = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "current desktop only", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	g_bDemandsAttentionWithDialog = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with dialog", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	g_bDemandsAttentionWithAnimation = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with animation", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	g_bAnimateOnActiveWindow = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "animate on active window", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	g_bOverWriteXIcons = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "overwrite xicon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	g_bShowThumbnail = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "window thumbnail", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	g_bMixLauncherAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "mix launcher appli", &bFlushConfFileNeeded, TRUE, NULL, NULL);
GET_GROUP_CONFIG_END*/


/*static gchar *s_cVisibleZoneImageFile = NULL;
GET_GROUP_CONFIG_BEGIN (HiddenDock)
	s_cVisibleZoneImageFile = cairo_dock_get_string_key_value (pKeyFile, "Hidden dock", "callback image", &bFlushConfFileNeeded, NULL, "Background", "background image");

	g_iVisibleZoneWidth = cairo_dock_get_integer_key_value (pKeyFile, "Hidden dock", "zone width", &bFlushConfFileNeeded, 150, "Background", NULL);
	if (g_iVisibleZoneWidth < 10) g_iVisibleZoneWidth = 10;
	g_iVisibleZoneHeight = cairo_dock_get_integer_key_value (pKeyFile, "Hidden dock", "zone height", &bFlushConfFileNeeded, 25, "Background", NULL);
	if (g_iVisibleZoneHeight < 1) g_iVisibleZoneHeight = 1;

	g_fVisibleZoneAlpha = cairo_dock_get_double_key_value (pKeyFile, "Hidden dock", "alpha", &bFlushConfFileNeeded, 0.5, "Background", NULL);
	g_bReverseVisibleImage = cairo_dock_get_boolean_key_value (pKeyFile, "Hidden dock", "reverse visible image", &bFlushConfFileNeeded, TRUE, "Background", NULL);
GET_GROUP_CONFIG_END*/


GET_GROUP_CONFIG_BEGIN (Background)
	g_iDockRadius = cairo_dock_get_integer_key_value (pKeyFile, "Background", "corner radius", &bFlushConfFileNeeded, 12, NULL, NULL);

	g_iDockLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Background", "line width", &bFlushConfFileNeeded, 2, NULL, NULL);

	g_iFrameMargin = cairo_dock_get_integer_key_value (pKeyFile, "Background", "frame margin", &bFlushConfFileNeeded, 2, NULL, NULL);

	double couleur[4] = {0., 0., 0.6, 0.4};
	cairo_dock_get_double_list_key_value (pKeyFile, "Background", "line color", &bFlushConfFileNeeded, g_fLineColor, 4, couleur, NULL, NULL);

	g_bRoundedBottomCorner = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "rounded bottom corner", &bFlushConfFileNeeded, TRUE, NULL, NULL);

	double couleur2[4] = {.7, .9, .7, .4};
	cairo_dock_get_double_list_key_value (pKeyFile, "Background", "stripes color bright", &bFlushConfFileNeeded, g_fStripesColorBright, 4, couleur2, NULL, NULL);

	g_free (g_cBackgroundImageFile);
	g_cBackgroundImageFile = cairo_dock_get_string_key_value (pKeyFile, "Background", "background image", &bFlushConfFileNeeded, NULL, NULL, NULL);

	g_fBackgroundImageAlpha = cairo_dock_get_double_key_value (pKeyFile, "Background", "image alpha", &bFlushConfFileNeeded, 0.5, NULL, NULL);

	g_bBackgroundImageRepeat = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "repeat image", &bFlushConfFileNeeded, FALSE, NULL, NULL);

	g_iNbStripes = cairo_dock_get_integer_key_value (pKeyFile, "Background", "number of stripes", &bFlushConfFileNeeded, 10, NULL, NULL);

	g_fStripesWidth = cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes width", &bFlushConfFileNeeded, 0.02, NULL, NULL);
	if (g_iNbStripes > 0 && g_fStripesWidth > 1. / g_iNbStripes)
	{
		cd_warning ("the stripes' width is greater than the space between them.");
		g_fStripesWidth = 0.99 / g_iNbStripes;
	}

	double couleur3[4] = {.7, .7, 1., .7};
	cairo_dock_get_double_list_key_value (pKeyFile, "Background", "stripes color dark", &bFlushConfFileNeeded, g_fStripesColorDark, 4, couleur3, NULL, NULL);

	g_fStripesAngle = cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes angle", &bFlushConfFileNeeded, 30., NULL, NULL);
GET_GROUP_CONFIG_END


GET_GROUP_CONFIG_BEGIN (Labels)
	g_free (g_iconTextDescription.cFont);
	g_iconTextDescription.cFont = cairo_dock_get_string_key_value (pKeyFile, "Labels", "police", &bFlushConfFileNeeded, "sans", "Icons", NULL);

	g_iconTextDescription.iSize = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "size", &bFlushConfFileNeeded, 14, "Icons", NULL);
	
	int iLabelWeight = cairo_dock_get_integer_key_value (pKeyFile, "Labels", "weight", &bFlushConfFileNeeded, 5, "Icons", NULL);
	g_iconTextDescription.iWeight = ((PANGO_WEIGHT_HEAVY - PANGO_WEIGHT_ULTRALIGHT) * iLabelWeight + 9 * PANGO_WEIGHT_ULTRALIGHT - PANGO_WEIGHT_HEAVY) / 8;  // on se ramene aux intervalles definit par Pango.
	
	gboolean bLabelStyleItalic = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "italic", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	if (bLabelStyleItalic)
		g_iconTextDescription.iStyle  = PANGO_STYLE_ITALIC;
	else
		g_iconTextDescription.iStyle  = PANGO_STYLE_NORMAL;
	

	if (g_iconTextDescription.cFont == NULL)
		g_iconTextDescription.iSize = 0;

	if (g_iconTextDescription.iSize == 0)
	{
		g_free (g_iconTextDescription.cFont);
		g_iconTextDescription.cFont = NULL;
	}
	
	double couleur_label[3] = {1., 1., 1.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color start", &bFlushConfFileNeeded, g_iconTextDescription.fColorStart, 3, couleur_label, "Icons", NULL);
	
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text color stop", &bFlushConfFileNeeded, g_iconTextDescription.fColorStop, 3, couleur_label, "Icons", NULL);
	
	g_iconTextDescription.bVerticalPattern = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "vertical label pattern", &bFlushConfFileNeeded, TRUE, "Icons", NULL);

	double couleur_backlabel[4] = {0., 0., 0., 0.5};
	cairo_dock_get_double_list_key_value (pKeyFile, "Labels", "text background color", &bFlushConfFileNeeded, g_iconTextDescription.fBackgroundColor, 4, couleur_backlabel, "Icons", NULL);
	
	g_free (g_quickInfoTextDescription.cFont);
	memcpy (&g_quickInfoTextDescription, &g_iconTextDescription, sizeof (CairoDockLabelDescription));
	g_quickInfoTextDescription.cFont = g_strdup (g_iconTextDescription.cFont);
	g_quickInfoTextDescription.iSize = 12;
	g_quickInfoTextDescription.iWeight = PANGO_WEIGHT_HEAVY;
	g_quickInfoTextDescription.iStyle = PANGO_STYLE_NORMAL;
	
	gboolean bUseBackgroundForLabel = cairo_dock_get_boolean_key_value (pKeyFile, "Labels", "background for label", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	if (! bUseBackgroundForLabel)
		g_iconTextDescription.fBackgroundColor[3] = 0;  // ne sera pas pris en compte.
GET_GROUP_CONFIG_END


double s_fFieldDepth;
gboolean s_bMixAppletsAndLaunchers;
GET_GROUP_CONFIG_BEGIN (Icons)
	gsize length=0;
	gchar **cIconsTypesList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "icon's type order", &bFlushConfFileNeeded, &length, NULL, "Cairo Dock", NULL);
	if (cIconsTypesList != NULL && length > 0)
	{
		unsigned int i, j;
		for (i = 0; i < length; i ++)
		{
			for (j = 0; j < ((CAIRO_DOCK_NB_TYPES + 1) / 2); j ++)
			{
				if (strcmp (cIconsTypesList[i], s_cIconTypeNames[j]) == 0)
				{
					g_tIconTypeOrder[2*j] = 2 * i;
				}
			}
		}
	}
	g_strfreev (cIconsTypesList);
	
	s_bMixAppletsAndLaunchers = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "mix applets with launchers", &bFlushConfFileNeeded, FALSE , NULL, NULL);
	if (s_bMixAppletsAndLaunchers)
		g_tIconTypeOrder[CAIRO_DOCK_APPLET] = g_tIconTypeOrder[CAIRO_DOCK_LAUNCHER];
	
	s_fFieldDepth = cairo_dock_get_double_key_value (pKeyFile, "Icons", "field depth", &bFlushConfFileNeeded, 0.7, NULL, NULL);

	g_fAlbedo = cairo_dock_get_double_key_value (pKeyFile, "Icons", "albedo", &bFlushConfFileNeeded, .6, NULL, NULL);

	g_fAmplitude = cairo_dock_get_double_key_value (pKeyFile, "Icons", "amplitude", &bFlushConfFileNeeded, 1.0, NULL, NULL);

	g_iSinusoidWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "sinusoid width", &bFlushConfFileNeeded, 250, NULL, NULL);
	g_iSinusoidWidth = MAX (1, g_iSinusoidWidth);

	g_iIconGap = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "icon gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	g_iStringLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "string width", &bFlushConfFileNeeded, 0, NULL, NULL);

	gdouble couleur[4];
	cairo_dock_get_double_list_key_value (pKeyFile, "Icons", "string color", &bFlushConfFileNeeded, g_fStringColor, 4, couleur, NULL, NULL);

	g_fAlphaAtRest = cairo_dock_get_double_key_value (pKeyFile, "Icons", "alpha at rest", &bFlushConfFileNeeded, 1., NULL, NULL);
	
	if (g_pDefaultIconDirectory != NULL)
	{
		gpointer data;
		int i;
		for (i = 0; (g_pDefaultIconDirectory[2*i] != NULL || g_pDefaultIconDirectory[2*i+1] != NULL); i ++)
		{
			if (g_pDefaultIconDirectory[2*i] != NULL)
				g_free (g_pDefaultIconDirectory[2*i]);
			else
				g_object_unref (g_pDefaultIconDirectory[2*i+1]);
		}
	}
	gchar **directoryList = cairo_dock_get_string_list_key_value (pKeyFile, "Icons", "default icon directory", &bFlushConfFileNeeded, &length, NULL, "Launchers", NULL);
	if (directoryList == NULL)
	{
		g_pDefaultIconDirectory = NULL;
	}
	else
	{
		g_pDefaultIconDirectory = g_new0 (gpointer, 2 * length + 2);  // +2 pour les NULL final.
		int i = 0, j = 0;
		while (directoryList[i] != NULL)
		{
			if (directoryList[i][0] == '~')
			{
				g_pDefaultIconDirectory[j] = g_strdup_printf ("%s%s", getenv ("HOME"), directoryList[i]+1);
				cd_message (" utilisation du repertoire %s", g_pDefaultIconDirectory[j]);
			}
			else if (directoryList[i][0] == '/')
			{
				g_pDefaultIconDirectory[j] = g_strdup (directoryList[i]);
				cd_message (" utilisation du repertoire %s", g_pDefaultIconDirectory[j]);
			}
			else if (strncmp (directoryList[i], CAIRO_DOCK_LOCAL_THEME_KEYWORD, strlen (CAIRO_DOCK_LOCAL_THEME_KEYWORD)) == 0)
			{
				g_pDefaultIconDirectory[j] = g_strdup_printf ("%s/%s%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR, directoryList[i]+strlen (CAIRO_DOCK_LOCAL_THEME_KEYWORD));
				cd_message (" utilisation du repertoire local %s", g_pDefaultIconDirectory[j]);
			}
			else if (strncmp (directoryList[i], "_ThemeDirectory_", 16) == 0)
			{
				g_pDefaultIconDirectory[j] = g_strdup_printf ("%s/%s%s", g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR, directoryList[i]+16);
				cd_message (" utilisation du nouveau repertoire local %s", g_pDefaultIconDirectory[j]);
				cd_warning ("Cairo-Dock's local icons are now located in the 'icons' folder, they will be moved there");
				gchar *cCommand = g_strdup_printf ("cd '%s' && mv *.svg *.png *.xpm *.jpg *.bmp *.gif '%s/%s' > /dev/null", g_cCurrentLaunchersPath, g_cCurrentThemePath, CAIRO_DOCK_LOCAL_ICONS_DIR);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				cCommand = g_strdup_printf ("sed -i \"s/_ThemeDirectory_/%s/g\" '%s/%s'", CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
				cCommand = g_strdup_printf ("sed -i \"/default icon directory/ { s/~\\/.config\\/%s\\/%s\\/icons/%s/g }\" '%s/%s'", CAIRO_DOCK_DATA_DIR, CAIRO_DOCK_CURRENT_THEME_NAME, CAIRO_DOCK_LOCAL_THEME_KEYWORD, g_cCurrentThemePath, CAIRO_DOCK_CONF_FILE);
				cd_message ("%s", cCommand);
				system (cCommand);
				g_free (cCommand);
			}
			else
			{
				cd_message (" utilisation du theme %s", directoryList[i]);
				g_pDefaultIconDirectory[j+1] = gtk_icon_theme_new ();
				gtk_icon_theme_set_custom_theme (g_pDefaultIconDirectory[j+1], directoryList[i]);
			}
			i ++;
			j += 2;
		}
	}
	g_strfreev (directoryList);

	//\___________________ Parametres des lanceurs.
	g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "launcher width", &bFlushConfFileNeeded, 48, "Launchers", "max icon size");

	g_tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "launcher height", &bFlushConfFileNeeded, 48, "Launchers", "max icon size");

	g_tAnimationType[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_animation_type_key_value (pKeyFile, "Icons", "launcher animation", &bFlushConfFileNeeded, CAIRO_DOCK_BOUNCE, "Launchers", "animation type");

	g_tNbAnimationRounds[CAIRO_DOCK_LAUNCHER] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "launcher number of rounds", &bFlushConfFileNeeded, 3, "Launchers", "number of animation rounds");

	//\___________________ Parametres des applis.
	g_tIconAuthorizedWidth[CAIRO_DOCK_APPLI] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "appli width", &bFlushConfFileNeeded, 48, "Applications", "max icon size");

	g_tIconAuthorizedHeight[CAIRO_DOCK_APPLI] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "appli height", &bFlushConfFileNeeded, 48, "Applications", "max icon size");

	g_tAnimationType[CAIRO_DOCK_APPLI] = cairo_dock_get_animation_type_key_value (pKeyFile, "Icons", "appli animation", &bFlushConfFileNeeded, CAIRO_DOCK_ROTATE, "Applications", "animation type");

	g_tNbAnimationRounds[CAIRO_DOCK_APPLI] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "appli number of rounds", &bFlushConfFileNeeded, 2, "Applications", "number of animation rounds");
	
	//\___________________ Parametres des applets.
	g_tIconAuthorizedWidth[CAIRO_DOCK_APPLET] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "applet width", &bFlushConfFileNeeded, 48, "Applets", "max icon size");

	g_tIconAuthorizedHeight[CAIRO_DOCK_APPLET] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "applet height", &bFlushConfFileNeeded, 48, "Applets", "max icon size");

	g_tAnimationType[CAIRO_DOCK_APPLET] = cairo_dock_get_animation_type_key_value (pKeyFile, "Icons", "applet animation", &bFlushConfFileNeeded, CAIRO_DOCK_ROTATE, "Applets", "animation type");

	g_tNbAnimationRounds[CAIRO_DOCK_APPLET] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "applet number of rounds", &bFlushConfFileNeeded, 2, "Applets", "number of animation rounds");
	
	//\___________________ Parametres des separateurs.
	g_tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator width", &bFlushConfFileNeeded, 48, "Separators", "min icon size");
	g_tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR23] = g_tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];

	g_tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12] = cairo_dock_get_integer_key_value (pKeyFile, "Icons", "separator height", &bFlushConfFileNeeded, 48, "Separators", "min icon size");
	g_tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR23] = g_tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];

	g_bUseSeparator = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "use separator", &bFlushConfFileNeeded, TRUE, "Separators", NULL);

	g_free (g_cSeparatorImage);
	g_cSeparatorImage = cairo_dock_get_string_key_value (pKeyFile, "Icons", "separator image", &bFlushConfFileNeeded, NULL, "Separators", NULL);

	g_bRevolveSeparator = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "revolve separator image", &bFlushConfFileNeeded, TRUE, "Separators", NULL);

	g_bConstantSeparatorSize = cairo_dock_get_boolean_key_value (pKeyFile, "Icons", "force size", &bFlushConfFileNeeded, TRUE, "Separators", NULL);
GET_GROUP_CONFIG_END


/*gchar *s_cIndicatorImagePath = NULL, *s_cDropIndicatorImagePath = NULL, *s_cActiveIndicatorImagePath = NULL;
double s_fIndicatorRatio;
double s_fActiveColor[4];
int s_iActiveLineWidth, s_iActiveCornerRadius;
GET_GROUP_CONFIG_BEGIN (Indicators)
	double couleur_active[4] = {0., 0.4, 0.8, 0.25};
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, s_fActiveColor, 4, couleur_active, "Icons", NULL);
	
	s_iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
	s_iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	g_bActiveIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "active frame position", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	gchar *cActiveIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (cActiveIndicatorImageName != NULL)
	{
		s_cActiveIndicatorImagePath = cairo_dock_generate_file_path (cActiveIndicatorImageName);
		g_free (cActiveIndicatorImageName);
	}
	else
		s_cActiveIndicatorImagePath = NULL;
	
	gchar *cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "indicator image", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cIndicatorImageName != NULL)
	{
		s_cIndicatorImagePath = cairo_dock_generate_file_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
	{
		s_cIndicatorImagePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_DEFAULT_INDICATOR_NAME);
	}
	
	g_bIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator above", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	s_fIndicatorRatio = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator ratio", &bFlushConfFileNeeded, 1., "Icons", NULL);
	
	g_bLinkIndicatorWithIcon = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "link indicator", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	
	g_iIndicatorDeltaY = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "indicator deltaY", &bFlushConfFileNeeded, 2, "Icons", NULL);
	
	gchar *cDropIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "drop indicator", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cDropIndicatorImageName != NULL)
	{
		s_cDropIndicatorImagePath = cairo_dock_generate_file_path (cDropIndicatorImageName);
		g_free (cDropIndicatorImageName);
	}
	else
	{
		s_cDropIndicatorImagePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_DEFAULT_DROP_INDICATOR_NAME);
	}
GET_GROUP_CONFIG_END*/


/*static gchar *s_cButtonCancelImage;
static gchar *s_cButtonOkImage;
GET_GROUP_CONFIG_BEGIN (Dialogs)
	s_cButtonOkImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_ok image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	s_cButtonCancelImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_cancel image", &bFlushConfFileNeeded, NULL, NULL, NULL);

	g_iDialogButtonWidth = cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "button width", &bFlushConfFileNeeded, 48, NULL, NULL);
	g_iDialogButtonHeight = cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "button height", &bFlushConfFileNeeded, 32, NULL, NULL);

	double couleur_bulle[4] = {1.0, 1.0, 1.0, 0.7};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "background color", &bFlushConfFileNeeded, g_fDialogColor, 4, couleur_bulle, NULL, NULL);

	g_iDialogIconSize = cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "icon size", &bFlushConfFileNeeded, 48, NULL, NULL);

	g_free (g_dialogTextDescription.cFont);
	if (cairo_dock_get_boolean_key_value (pKeyFile, "Dialogs", "homogeneous text", &bFlushConfFileNeeded, TRUE, NULL, NULL))
	{
		g_dialogTextDescription.iSize = g_iconTextDescription.iSize;
		if (g_dialogTextDescription.iSize == 0)
			g_dialogTextDescription.iSize = 14;
		g_dialogTextDescription.cFont = g_strdup (g_iconTextDescription.cFont);
		g_dialogTextDescription.iWeight = g_iconTextDescription.iWeight;
		g_dialogTextDescription.iStyle = g_iconTextDescription.iStyle;
	}
	else
	{
		g_dialogTextDescription.cFont = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "message police", &bFlushConfFileNeeded, "sans", NULL, NULL);
		g_dialogTextDescription.iSize = cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "message size", &bFlushConfFileNeeded, 14, NULL, NULL);
		int iLabelWeight = cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "message weight", &bFlushConfFileNeeded, 5, NULL, NULL);
		g_dialogTextDescription.iWeight = ((PANGO_WEIGHT_HEAVY - PANGO_WEIGHT_ULTRALIGHT) * iLabelWeight + 9 * PANGO_WEIGHT_ULTRALIGHT - PANGO_WEIGHT_HEAVY) / 8;  // on se ramene aux intervalles definit par Pango.
		if (cairo_dock_get_boolean_key_value (pKeyFile, "Dialogs", "message italic", &bFlushConfFileNeeded, FALSE, NULL, NULL))
			g_dialogTextDescription.iStyle = PANGO_STYLE_ITALIC;
		else
			g_dialogTextDescription.iStyle = PANGO_STYLE_NORMAL;
	}
	
	double couleur_dtext[4] = {0., 0., 0., 1.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "text color", &bFlushConfFileNeeded, g_dialogTextDescription.fColorStart, 3, couleur_dtext, NULL, NULL);
	memcpy (&g_dialogTextDescription.fColorStop, &g_dialogTextDescription.fColorStart, 3*sizeof (double));
GET_GROUP_CONFIG_END*/


/*GET_GROUP_CONFIG_BEGIN (Views)
	g_free (g_cMainDockDefaultRendererName);
	g_cMainDockDefaultRendererName = cairo_dock_get_string_key_value (pKeyFile, "Views", "main dock view", &bFlushConfFileNeeded, CAIRO_DOCK_DEFAULT_RENDERER_NAME, "Cairo Dock", NULL);
	if (g_cMainDockDefaultRendererName == NULL)
		g_cMainDockDefaultRendererName = g_strdup (CAIRO_DOCK_DEFAULT_RENDERER_NAME);
	cd_message ("g_cMainDockDefaultRendererName <- %s", g_cMainDockDefaultRendererName);

	g_free (g_cSubDockDefaultRendererName);
	g_cSubDockDefaultRendererName = cairo_dock_get_string_key_value (pKeyFile, "Views", "sub-dock view", &bFlushConfFileNeeded, CAIRO_DOCK_DEFAULT_RENDERER_NAME, "Sub-Docks", NULL);
	if (g_cSubDockDefaultRendererName == NULL)
		g_cSubDockDefaultRendererName = g_strdup (CAIRO_DOCK_DEFAULT_RENDERER_NAME);

	g_bSameHorizontality = cairo_dock_get_boolean_key_value (pKeyFile, "Views", "same horizontality", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);

	g_fSubDockSizeRatio = cairo_dock_get_double_key_value (pKeyFile, "Views", "relative icon size", &bFlushConfFileNeeded, 0.8, "Sub-Docks", NULL);
GET_GROUP_CONFIG_END*/


GET_GROUP_CONFIG_BEGIN (Desklets)
	g_cDeskletDecorationsName = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "decorations", &bFlushConfFileNeeded, "dark", NULL, NULL);
	CairoDeskletDecoration *pUserDeskletDecorations = cairo_dock_get_desklet_decoration ("personnal");
	if (pUserDeskletDecorations == NULL)
	{
		pUserDeskletDecorations = g_new0 (CairoDeskletDecoration, 1);
		cairo_dock_register_desklet_decoration ("personnal", pUserDeskletDecorations);
	}
	if (g_cDeskletDecorationsName == NULL || strcmp (g_cDeskletDecorationsName, "personnal") == 0)
	{
		g_free (pUserDeskletDecorations->cBackGroundImagePath);
		pUserDeskletDecorations->cBackGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "bg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		g_free (pUserDeskletDecorations->cForeGroundImagePath);
		pUserDeskletDecorations->cForeGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "fg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pUserDeskletDecorations->iLoadingModifier = CAIRO_DOCK_FILL_SPACE;
		pUserDeskletDecorations->fBackGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "bg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->fForeGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "fg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->iLeftMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "left offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iTopMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "top offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iRightMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "right offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
		pUserDeskletDecorations->iBottomMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "bottom offset", &bFlushConfFileNeeded, CAIRO_DOCK_FM_SORT_BY_NAME, NULL, NULL);
	}
	g_iDeskletButtonSize = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "button size", &bFlushConfFileNeeded, 16, NULL, NULL);
	g_free (g_cRotateButtonImage);
	g_cRotateButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "rotate image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	g_free (g_cRetachButtonImage);
	g_cRetachButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "retach image", &bFlushConfFileNeeded, NULL, NULL, NULL);
GET_GROUP_CONFIG_END


void cairo_dock_read_conf_file (gchar *cConfFilePath, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, cConfFilePath);
	GError *erreur = NULL;
	gsize length;
	gboolean bFlushConfFileNeeded = FALSE, bFlushConfFileNeeded2 = FALSE;  // si un champ n'existe pas, on le rajoute au fichier de conf.

	//\___________________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		return ;
	}
	
	s_bLoading = TRUE;
	
	//\___________________ On garde une trace de certains parametres.
	gchar *cRaiseDockShortcutOld = myAccessibility.cRaiseDockShortcut;
	myAccessibility.cRaiseDockShortcut = NULL;
	gboolean bPopUpOld = myAccessibility.bPopUp;  // FALSE initialement.
	gboolean bUseFakeTransparencyOld = mySystem.bUseFakeTransparency;  // FALSE initialement.
	gboolean bUniquePidOld = myTaskBar.bUniquePid;  // FALSE initialement.
	gboolean bGroupAppliByClassOld = myTaskBar.bGroupAppliByClass;  // FALSE initialement.
	gboolean bHideVisibleApplisOld = myTaskBar.bHideVisibleApplis;
	gboolean bAppliOnCurrentDesktopOnlyOld = myTaskBar.bAppliOnCurrentDesktopOnly;
	gboolean bMixLauncherAppliOld = myTaskBar.bMixLauncherAppli;
	gboolean bOverWriteXIconsOld = myTaskBar.bOverWriteXIcons;  // TRUE initialement.
	gboolean bShowThumbnailOld = myTaskBar.bShowThumbnail;
	gchar *cIndicatorImagePathOld = myIndicators.cIndicatorImagePath;
	gchar *cDropIndicatorImagePathOld = myIndicators.cDropIndicatorImagePath;
	gchar *cActiveIndicatorImagePathOld = myIndicators.cActiveIndicatorImagePath;
	
	//\___________________ On recupere la conf de tous les modules.
	bFlushConfFileNeeded = cairo_dock_get_global_config (pKeyFile);
	
	
	//\___________________ On recupere la position du dock.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Position);
	
	//\___________________ On recupere les parametres d'accessibilite.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Accessibility);
	
	//\___________________ On recupere les parametres systeme.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (System);
	
	//\___________________ On recupere les parametres de la barre des taches.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (TaskBar);
	
	//\___________________ On recupere les parametres de la zone visible.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (HiddenDock);
	
	//\___________________ On recupere les parametres de dessin du fond.
	bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Background);
	
	//\___________________ On recupere les parametres des etiquettes.
	bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Labels);
	
	//\___________________ On recupere les parametres des icones.
	gboolean bMixAppletsAndLaunchersOld = (g_tIconTypeOrder[CAIRO_DOCK_APPLET] == g_tIconTypeOrder[CAIRO_DOCK_LAUNCHER]);
	gboolean bUseSeparatorOld = g_bUseSeparator;
	int tIconTypeOrderOld[CAIRO_DOCK_NB_TYPES];
	memcpy (tIconTypeOrderOld, g_tIconTypeOrder, sizeof (tIconTypeOrderOld));
	bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Icons);
	
	//\___________________ On recupere les parametres des indicateurs.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Indicators);
	
	//\___________________ On recupere les parametres des dialogues.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Dialogs);
	
	//\___________________ On recupere les parametres des desklets.
	gchar *cDeskletDecorationsNameOld = g_cDeskletDecorationsName;
	bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Desklets);
	
	//\___________________ On recupere les parametres des vues.
	//bFlushConfFileNeeded |= cairo_dock_read_group_conf_file (Views);
	
	
	//\___________________ Post-initialisation.
	pDock->iGapX = myPosition.iGapX;
	pDock->iGapY = myPosition.iGapY;
	
	pDock->fAlign = myPosition.fAlign;
	
	myTaskBar.bAutoHideOnFullScreen = myTaskBar.bAutoHideOnFullScreen && (!pDock->bAutoHide);
	myTaskBar.bAutoHideOnMaximized = myTaskBar.bAutoHideOnMaximized && (!pDock->bAutoHide);
	
	gboolean bGroupOrderChanged;
	if (tIconTypeOrderOld[CAIRO_DOCK_LAUNCHER] != g_tIconTypeOrder[CAIRO_DOCK_LAUNCHER] ||
		tIconTypeOrderOld[CAIRO_DOCK_APPLI] != g_tIconTypeOrder[CAIRO_DOCK_APPLI] ||
		tIconTypeOrderOld[CAIRO_DOCK_APPLET] != g_tIconTypeOrder[CAIRO_DOCK_APPLET])
		bGroupOrderChanged = TRUE;
	else
		bGroupOrderChanged = FALSE;
	
	cairo_dock_updated_emblem_conf_file (pKeyFile, &bFlushConfFileNeeded);
	
	//\___________________ On (re)charge tout, car n'importe quel parametre peut avoir change.
	switch (myPosition.iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
			pDock->bHorizontalDock = CAIRO_DOCK_HORIZONTAL;
			pDock->bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_TOP :
			pDock->bHorizontalDock = CAIRO_DOCK_HORIZONTAL;
			pDock->bDirectionUp = FALSE;
		break;
		case CAIRO_DOCK_RIGHT :
			pDock->bHorizontalDock = CAIRO_DOCK_VERTICAL;
			pDock->bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_LEFT :
			pDock->bHorizontalDock = CAIRO_DOCK_VERTICAL;
			pDock->bDirectionUp = FALSE;
		break;
	}
	
	if (myAccessibility.iMaxAuthorizedWidth == 0 || myAccessibility.iMaxAuthorizedWidth > g_iScreenWidth[pDock->bHorizontalDock])
		myAccessibility.iMaxAuthorizedWidth = g_iScreenWidth[pDock->bHorizontalDock];
	
	
	cairo_t* pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	
	if (mySystem.bUseFakeTransparency && g_pDesktopBgSurface == NULL)
		cairo_dock_load_desktop_background_surface ();
	
	cairo_dock_load_dialog_buttons (CAIRO_CONTAINER (pDock), myDialogs.cButtonOkImage, myDialogs.cButtonCancelImage);
	
	cairo_dock_load_task_indicator (myTaskBar.bShowAppli && myTaskBar.bMixLauncherAppli ? myIndicators.cIndicatorImagePath : NULL, pCairoContext, fMaxScale, myIndicators.fIndicatorRatio);
	
	cairo_dock_load_drop_indicator (myIndicators.cDropIndicatorImagePath, pCairoContext, fMaxScale);
	
	cairo_dock_load_active_window_indicator (pCairoContext, myIndicators.cActiveIndicatorImagePath, fMaxScale, myIndicators.iActiveCornerRadius, myIndicators.iActiveLineWidth, myIndicators.fActiveColor);
	
	cairo_dock_load_desklet_buttons (pCairoContext);
	
	cairo_dock_load_visible_zone (pDock, myHiddenDock.cVisibleZoneImageFile, myHiddenDock.iVisibleZoneWidth, myHiddenDock.iVisibleZoneHeight, myHiddenDock.fVisibleZoneAlpha);
	
	g_fReflectSize = 0;
	guint i;
	for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
	{
		if (g_tIconAuthorizedHeight[i] > 0)
			g_fReflectSize = MAX (g_fReflectSize, g_tIconAuthorizedHeight[i]);
	}
	if (g_fReflectSize == 0)  // on n'a pas trouve de hauteur, on va essayer avec la largeur.
	{
		for (i = 0; i < CAIRO_DOCK_NB_TYPES; i ++)
		{
			if (g_tIconAuthorizedWidth[i] > 0)
				g_fReflectSize = MAX (g_fReflectSize, g_tIconAuthorizedWidth[i]);
		}
		if (g_fReflectSize > 0)
			g_fReflectSize = MIN (48, g_fReflectSize);
		else
			g_fReflectSize = 48;
	}
	g_fReflectSize *= s_fFieldDepth;
	cd_debug ("  g_fReflectSize : %.2f pixels", g_fReflectSize);
	
	if (myTaskBar.bShowThumbnail && ! bShowThumbnailOld)  // on verifie que cette option est acceptable.
	{
		if (! cairo_dock_support_X_extension ())
		{
			cd_warning ("Sorry but your X server does not support the extension.\n You can't have window thumbnails in the dock");
			myTaskBar.bShowThumbnail = FALSE;
		}
		
	}
	if (bUniquePidOld != myTaskBar.bUniquePid || bGroupAppliByClassOld != myTaskBar.bGroupAppliByClass || bHideVisibleApplisOld != myTaskBar.bHideVisibleApplis || bAppliOnCurrentDesktopOnlyOld != myTaskBar.bAppliOnCurrentDesktopOnly || (bMixLauncherAppliOld != myTaskBar.bMixLauncherAppli) || (bOverWriteXIconsOld != myTaskBar.bOverWriteXIcons) || (bShowThumbnailOld != myTaskBar.bShowThumbnail) || (cairo_dock_application_manager_is_running () && ! myTaskBar.bShowAppli))  // on ne veut plus voir les applis, il faut donc les enlever.
	{
		cairo_dock_stop_application_manager ();
	}
	
	if (bGroupOrderChanged || s_bMixAppletsAndLaunchers != bMixAppletsAndLaunchersOld)
		pDock->icons = g_list_sort (pDock->icons, (GCompareFunc) cairo_dock_compare_icons_order);

	if ((bUseSeparatorOld && ! g_bUseSeparator) || (! bMixAppletsAndLaunchersOld && s_bMixAppletsAndLaunchers) || bGroupOrderChanged)
		cairo_dock_remove_all_separators (pDock);
		
	g_fBackgroundImageWidth = 1e4;  // inutile de mettre a jour les decorations maintenant.
	g_fBackgroundImageHeight = 1e4;
	if (pDock->icons == NULL)
	{
		pDock->fFlatDockWidth = - g_iIconGap;  // car on ne le connaissait pas encore au moment de la creation du dock.
		cairo_dock_build_docks_tree_with_desktop_files (pDock, g_cCurrentLaunchersPath);
	}
	else
	{
		cairo_dock_synchronize_sub_docks_position (pDock, FALSE);
		cairo_dock_reload_buffers_in_all_docks (FALSE);  // tout sauf les applets, qui seront rechargees en bloc juste apres.
	}


	if (! cairo_dock_application_manager_is_running () && myTaskBar.bShowAppli)  // maintenant on veut voir les applis !
	{
		cairo_dock_start_application_manager (pDock);  // va inserer le separateur si necessaire.
	}

	if ((g_bUseSeparator && ! bUseSeparatorOld) || (bMixAppletsAndLaunchersOld != s_bMixAppletsAndLaunchers) || bGroupOrderChanged)
	{
		cairo_dock_insert_separators_in_dock (pDock);
	}
	
	GTimeVal time_val;
	g_get_current_time (&time_val);  // on pourrait aussi utiliser un compteur statique a la fonction ...
	double fTime = time_val.tv_sec + time_val.tv_usec * 1e-6;
	cairo_dock_activate_modules_from_list (mySystem.cActiveModuleList, fTime);
	cairo_dock_deactivate_old_modules (fTime);

	cairo_dock_set_all_views_to_default (0);  // met a jour la taille de tous les docks, maintenant qu'ils sont tous remplis.
	cairo_dock_redraw_root_docks (TRUE);  // TRUE <=> sauf le main dock.
	
	if (myAccessibility.cRaiseDockShortcut != NULL)
	{
		if (cRaiseDockShortcutOld == NULL || strcmp (myAccessibility.cRaiseDockShortcut, cRaiseDockShortcutOld) != 0)
		{
			if (cRaiseDockShortcutOld != NULL)
				cd_keybinder_unbind (cRaiseDockShortcutOld, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
			if (! cd_keybinder_bind (myAccessibility.cRaiseDockShortcut, (CDBindkeyHandler) cairo_dock_raise_from_keyboard, NULL))
			{
				g_free (myAccessibility.cRaiseDockShortcut);
				myAccessibility.cRaiseDockShortcut = NULL;
			}
		}
	}
	else
	{
		if (cRaiseDockShortcutOld != NULL)
		{
			cd_keybinder_unbind (cRaiseDockShortcutOld, (CDBindkeyHandler) cairo_dock_raise_from_keyboard);
			cairo_dock_place_root_dock (pDock);
			gtk_widget_show (pDock->pWidget);
		}
	}
	g_free (cRaiseDockShortcutOld);
	
	myAccessibility.bReserveSpace = myAccessibility.bReserveSpace && (myAccessibility.cRaiseDockShortcut == NULL);
	cairo_dock_reserve_space_for_all_root_docks (myAccessibility.bReserveSpace);

	cairo_dock_load_background_decorations (pDock);


	pDock->iMouseX = 0;  // on se place hors du dock initialement.
	pDock->iMouseY = 0;
	pDock->calculate_icons (pDock);
	gtk_widget_queue_draw (pDock->pWidget);  // le 'gdk_window_move_resize' ci-dessous ne provoquera pas le redessin si la taille n'a pas change.

	cairo_dock_place_root_dock (pDock);
	
	if (cDeskletDecorationsNameOld == NULL && g_cDeskletDecorationsName != NULL)  // chargement initial, on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE, pCairoContext);
	}
	else if (cDeskletDecorationsNameOld != NULL && (g_cDeskletDecorationsName == NULL || strcmp (cDeskletDecorationsNameOld, g_cDeskletDecorationsName) != 0))  // le theme par defaut a change, on recharge les desklets qui utilisent le theme "default".
	{
		cairo_dock_reload_desklets_decorations (TRUE, pCairoContext);
	}
	else if (g_cDeskletDecorationsName != NULL && strcmp (g_cDeskletDecorationsName, "personnal") == 0)  // on a configure le theme personnel, il peut avoir change.
	{
		cairo_dock_reload_desklets_decorations (TRUE, pCairoContext);
	}
	else  // on charge juste ceux qui n'ont pas encore leur deco et qui ont atteint leur taille definitive.
	{
		cairo_dock_reload_desklets_decorations (FALSE, pCairoContext);
	}
	g_free (cDeskletDecorationsNameOld);
	
	cairo_dock_update_renderer_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_gui ();
	cairo_dock_update_desklet_decorations_list_for_applet_gui ();
	
	
	if (myAccessibility.bPopUp)
	{
		cairo_dock_start_polling_screen_edge (pDock);
		if (! bPopUpOld)
			gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), TRUE);  // le main dock ayant ete cree avant, il n'a pas herite de ce parametre.
	}
	else
	{
		cairo_dock_stop_polling_screen_edge ();
		if (bPopUpOld)
			cairo_dock_set_root_docks_on_top_layer ();
	}
	
	if (mySystem.bUseFakeTransparency && ! bUseFakeTransparencyOld)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), TRUE);  // le main dock ayant ete cree avant, il n'a pas herite de ce parametre.
	else if (! mySystem.bUseFakeTransparency && bUseFakeTransparencyOld)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), FALSE);
	
	
	//\___________________ On ecrit si necessaire.
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, CAIRO_DOCK_VERSION);
	if (bFlushConfFileNeeded)
	{
		cairo_dock_flush_conf_file (pKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
		g_key_file_free (pKeyFile);
		pKeyFile = g_key_file_new ();
		g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
	}
	
	cairo_destroy (pCairoContext);
	
	g_key_file_free (pKeyFile);

	cairo_dock_mark_theme_as_modified (TRUE);
	
	s_bLoading = FALSE;
}

gboolean cairo_dock_is_loading (void)
{
	return s_bLoading;
}


void cairo_dock_update_conf_file (const gchar *cConfFilePath, GType iFirstDataType, ...)  // type, groupe, nom, valeur, etc. finir par G_TYPE_INVALID.
{
	cd_message ("%s (%s)", __func__, cConfFilePath);
	va_list args;
	va_start (args, iFirstDataType);

	GKeyFile *pKeyFile = g_key_file_new ();
	GError *erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cConfFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_key_file_free (pKeyFile);
		va_end (args);
		return ;
	}

	GType iType = iFirstDataType;
	gboolean bValue;
	gint iValue;
	double fValue;
	gchar *cValue;
	gchar *cGroupName, *cGroupKey;
	while (iType != G_TYPE_INVALID)
	{
		cGroupName = va_arg (args, gchar *);
		cGroupKey = va_arg (args, gchar *);

		switch (iType)
		{
			case G_TYPE_BOOLEAN :
				bValue = va_arg (args, gboolean);
				g_key_file_set_boolean (pKeyFile, cGroupName, cGroupKey, bValue);
			break ;
			case G_TYPE_INT :
				iValue = va_arg (args, gint);
				g_key_file_set_integer (pKeyFile, cGroupName, cGroupKey, iValue);
			break ;
			case G_TYPE_DOUBLE :
				fValue = va_arg (args, gdouble);
				g_key_file_set_double (pKeyFile, cGroupName, cGroupKey, fValue);
			break ;
			case G_TYPE_STRING :
				cValue = va_arg (args, gchar *);
				g_key_file_set_string (pKeyFile, cGroupName, cGroupKey, cValue);
			break ;
			default :
			break ;
		}

		iType = va_arg (args, GType);
	}

	cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
	g_key_file_free (pKeyFile);

	va_end (args);
}


void cairo_dock_update_conf_file_with_position (const gchar *cConfFilePath, int x, int y)
{
	//g_print ("%s (%s ; %d;%d)\n", __func__, cConfFilePath, x, y);
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Position", "x gap", x,
		G_TYPE_INT, "Position", "y gap", y,
		G_TYPE_INVALID);
}


CairoDockDesktopEnv cairo_dock_guess_environment (void)
{
	const gchar * cEnv = g_getenv ("GNOME_DESKTOP_SESSION_ID");
	CairoDockDesktopEnv iDesktopEnv;
	if (cEnv == NULL || *cEnv == '\0')
	{
		cEnv = g_getenv ("KDE_FULL_SESSION");
		if (cEnv == NULL || *cEnv == '\0')
		{
			if (! cairo_dock_property_is_present_on_root ("_DT_SAVE_MODE"))
				iDesktopEnv = CAIRO_DOCK_UNKNOWN_ENV;
			else
				iDesktopEnv = CAIRO_DOCK_XFCE;
		}
		else
			iDesktopEnv = CAIRO_DOCK_KDE;
	}
	else
		iDesktopEnv = CAIRO_DOCK_GNOME;
	
	return iDesktopEnv;
}

void cairo_dock_get_version_from_string (gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion)
{
	gchar **cVersions = g_strsplit (cVersionString, ".", -1);
	if (cVersions[0] != NULL)
	{
		*iMajorVersion = atoi (cVersions[0]);
		if (cVersions[1] != NULL)
		{
			*iMinorVersion = atoi (cVersions[1]);
			if (cVersions[2] != NULL)
				*iMicroVersion = atoi (cVersions[2]);
		}
	}
	g_strfreev (cVersions);
}
