/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CAIRO_DIALOG_MANAGER__
#define  __CAIRO_DIALOG_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"  // CairoDockLabelDescription
#include "cairo-dock-container.h"
G_BEGIN_DECLS

/** @file cairo-dock-dialog-manager.h This class manages the Dialogs, that are useful to bring interaction with the user.
* 
* With dialogs, you can pop-up messages, ask for question, etc. Any GTK widget can be embedded inside a dialog, giving you any possible interaction with the user.
* 
* The most generic way to build a Dialog is to fill a _CairoDialogAttr and pass it to \ref gldi_dialog_new.
* 
* But in most of case, you can just use one of the following convenient functions, that will do the job for you.
* - to show a message, you can use \ref gldi_dialog_show_temporary_with_icon
* - to ask the user a choice, a value or a text, you can use \ref gldi_dialog_show_with_question, \ref gldi_dialog_show_with_value or \ref gldi_dialog_show_with_entry.
* - if you want to pop up only 1 dialog at once on a given icon, use \ref gldi_dialogs_remove_on_icon before you pop up your dialog.
*/

typedef struct _CairoDialogsParam CairoDialogsParam;
typedef struct _CairoDialogsManager CairoDialogsManager;
typedef struct _CairoDialogAttr CairoDialogAttr;

#ifndef _MANAGER_DEF_
extern CairoDialogsParam myDialogsParam;
extern CairoDialogsManager myDialogsMgr;
#endif

// params
struct _CairoDialogsParam {
	gchar *cButtonOkImage;
	gchar *cButtonCancelImage;
	gint iDialogButtonWidth;
	gint iDialogButtonHeight;
	gint iDialogIconSize;
	CairoDockLabelDescription dialogTextDescription;
	gchar *cDecoratorName;
	gdouble fDialogColor[4];
};

// manager
struct _CairoDialogsManager {
	GldiManager mgr;
};

/// Definition of a generic callback of a dialog, called when the user clicks on a button. Buttons are numbered from 0, -1 means 'Return' and -2 means 'Escape'.
typedef void (* CairoDockActionOnAnswerFunc) (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog);

struct _CairoDialogAttr {
	// parent attributes
	GldiContainerAttr cattr;
	/// path to an image to display in the left margin, or NULL.
	const gchar *cImageFilePath;
	/// size of the icon in the left margin, or 0 to use the default one.
	gint iIconSize;
	/// text of the message, or NULL.
	const gchar *cText;
	/// whether to use Pango markups or not (markups are html-like marks, like <b>...</b>; using markups force you to escape some characters like "&" -> "&amp;")
	gboolean bUseMarkup;
	/// a widget to interact with the user, or NULL.
	GtkWidget *pInteractiveWidget;
	/// a NULL-terminated list of images for buttons, or NULL. "ok" and "cancel" are key word to load the default "ok" and "cancel" buttons.
	const gchar **cButtonsImage;
	/// function that will be called when the user click on a button, or NULL.
	CairoDockActionOnAnswerFunc pActionFunc;
	/// data passed as a parameter of the callback, or NULL.
	gpointer pUserData;
	/// a function to free the data when the dialog is destroyed, or NULL.
	GFreeFunc pFreeDataFunc;
	/// life time of the dialog (in ms), or 0 for an unlimited dialog.
	gint iTimeLength;
	/// name of a decorator, or NULL to use the default one.
	const gchar *cDecoratorName;
	/// whether the dialog should be transparent to mouse input.
	gboolean bNoInput;
	/// whether to pop-up the dialog in front of al other windows, including fullscreen windows.
	gboolean bForceAbove;
	/// for a dialog with no buttons, clicking on it will close it, or hide if this boolean is TRUE.
	gboolean bHideOnClick;
	/// icon it will be point to
	Icon *pIcon;
	/// container it will point to
	GldiContainer *pContainer;
};

/// signals
typedef enum {
	NB_NOTIFICATIONS_DIALOG = NB_NOTIFICATIONS_CONTAINER
	} CairoDialogNotifications;
	

/** Remove the dialogs attached to an icon.
*@param icon the icon you want to delete all dialogs from.
*/
void gldi_dialogs_remove_on_icon (Icon *icon);


void gldi_dialogs_refresh_all (void);

void gldi_dialogs_replace_all (void);


CairoDialog *gldi_dialogs_foreach (GCompareFunc callback, gpointer data);


/** Hide a dialog.
*@param pDialog the dialog.
*/
void gldi_dialog_hide (CairoDialog *pDialog);
/** Show a dialog and give it focus.
*@param pDialog the dialog.
*/
void gldi_dialog_unhide (CairoDialog *pDialog);
/** Toggle the visibility of a dialog.
*@param pDialog the dialog.
*/
void gldi_dialog_toggle_visibility (CairoDialog *pDialog);


void gldi_register_dialogs_manager (void);

G_END_DECLS
#endif
