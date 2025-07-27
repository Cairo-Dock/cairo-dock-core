/**
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

#include <stdlib.h>
#include <string.h>

#include "applet-struct.h"
#include "applet-composite.h"

static void (*s_activate_composite) (gboolean) = NULL;

  /////////////////////
 ///  WM Composite ///
/////////////////////

static void _set_metacity_composite (gboolean bActive)
{
	int r;
	if (bActive)
		r = system ("if test -n \"`dconf read /org/gnome/metacity/compositing-manager`\"; then dconf write /org/gnome/metacity/compositing-manager true; metacity --replace& else gsettings set org.gnome.metacity compositing-manager true; fi");  // metacity's new feature : now uses Dconf and crashes after activating the composite
	else
		r = system ("if test -n \"`dconf read /org/gnome/metacity/compositing-manager`\"; then dconf write /org/gnome/metacity/compositing-manager false; metacity --replace& else gsettings set org.gnome.metacity compositing-manager false; fi");
	if (r < 0)
		cd_warning ("Not able to change Metacity compositioning setting");
}

static void _set_xfwm_composite (gboolean bActive)
{
	int r;
	if (bActive)
		r = system ("xfconf-query -c xfwm4 -p '/general/use_compositing' -t 'bool' -s 'true'");
	else
		r = system ("xfconf-query -c xfwm4 -p '/general/use_compositing' -t 'bool' -s 'false'");
	if (r < 0)
		cd_warning ("Not able to launch this command: xfconf-query");
}

static void _set_kwin_composite (gboolean bActive)
{
	int r;
	if (bActive)
		r = system ("[ \"$(qdbus org.kde.kwin /KWin compositingActive)\" == \"false\" ] && qdbus org.kde.kwin /KWin toggleCompositing");  // not active, so activating
	else
		r = system ("[ \"$(qdbus org.kde.kwin /KWin compositingActive)\" == \"true\" ] && qdbus org.kde.kwin /KWin toggleCompositing");  // active, so deactivating
	if (r < 0)
		cd_warning ("Not able to launch this command: qdbus");
}

  ///////////////////////
 /// Welcome message ///
///////////////////////

static void cd_help_show_welcome_message (void)
{
	gldi_dialog_show (D_("Welcome in Cairo-Dock !\n"
		"This applet is here to help you start using the dock; just click on it.\n"
		"If you have any question/request/remark, please pay us a visit at\n"
		"https://github.com/Cairo-Dock/cairo-dock-core.\n"
		"Hope you will enjoy this software !\n"
		"  (you can now click on this dialog to close it)"),
		myIcon, myContainer,
		0,
		"same icon",
		NULL, NULL, NULL, NULL);
	myData.bFirstLaunch = FALSE;
}

  ////////////////////////
 /// Composite dialog ///
////////////////////////

static inline void _cancel_wm_composite (void)
{
	s_activate_composite (FALSE);
}
static void _accept_wm_composite (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, gpointer data, G_GNUC_UNUSED CairoDialog *pDialog)
{
	cd_debug ("%s (%d)", __func__, iClickedButton);
	if (iClickedButton == 1 || iClickedButton == -2)  // clic explicite sur "cancel", ou Echap ou auto-delete.
	{
		_cancel_wm_composite ();
	}
	gboolean *bAccepted = data;
	*bAccepted = TRUE;  // l'utilisateur a valide son choix.
}
static void _on_free_wm_dialog (gpointer data)
{
	gboolean *bAccepted = data;
	cd_debug ("%s (%d)", __func__, *bAccepted);
	if (! *bAccepted)  // le dialogue s'est detruit sans que l'utilisateur n'ait valide la question => on annule tout.
	{
		_cancel_wm_composite ();
	}
	g_free (data);
	
	if (myData.bFirstLaunch)
	{
		cd_help_show_welcome_message ();
	}
}

static void _toggle_remember_choice (GtkCheckButton *pButton, GtkWidget *pDialog)
{
	g_object_set_data (G_OBJECT (pDialog), "remember", GINT_TO_POINTER (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pButton))));
}

static void _on_free_info_dialog (G_GNUC_UNUSED gpointer data)
{
	if (myData.bFirstLaunch)
	{
		cd_help_show_welcome_message ();
	}
}

  ///////////////////////
 /// Check composite ///
///////////////////////

void cd_help_enable_composite (void)
{
	// find the current WM.
	s_activate_composite = NULL;
	gchar *cPsef = cairo_dock_launch_command_sync ("pgrep metacity");  // 'ps | grep' ne marche pas, il faut le lancer dans un script :-/
	cd_debug ("cPsef: '%s'", cPsef);
	if (cPsef != NULL && *cPsef != '\0')
	{
		s_activate_composite = _set_metacity_composite;
	}
	else
	{
		cPsef = cairo_dock_launch_command_sync ("pgrep xfwm");
		if (cPsef != NULL && *cPsef != '\0')
		{
			s_activate_composite = _set_xfwm_composite;
		}
		else
		{
			cPsef = cairo_dock_launch_command_sync ("pgrep kwin");
			if (cPsef != NULL && *cPsef != '\0')
			{
				s_activate_composite = _set_kwin_composite;
			}
		}
	}

	// if the WM can handle the composite, ask the user if he wants to enable it.
	if (s_activate_composite != NULL)  // the WM can activate the composite.
	{
		GtkWidget *pAskBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
		GtkWidget *label = gtk_label_new (D_("Don't ask me any more"));
		gldi_dialog_set_widget_text_color (label);
		GtkWidget *pCheckBox = gtk_check_button_new ();
		gtk_box_pack_end (GTK_BOX (pAskBox), pCheckBox, FALSE, FALSE, 0);
		gtk_box_pack_end (GTK_BOX (pAskBox), label, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (pCheckBox), "toggled", G_CALLBACK(_toggle_remember_choice), pAskBox);
		int iClickedButton = gldi_dialog_show_and_wait (D_("To remove the black rectangle around the dock, you need to activate a composite manager.\nDo you want to activate it now?"), myIcon, myContainer, NULL, pAskBox);

		gboolean bRememberChoice = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pCheckBox));
		gtk_widget_destroy (pAskBox);  // le widget survit a un dialogue bloquant.
		if (bRememberChoice)  // if not ticked, we'll check the composite on the next startup, and if it's ok, we'll not check it any more.
		{
			myData.bTestComposite = FALSE;
		}
		if (iClickedButton == 0 || iClickedButton == -1)  // ok or Enter.
		{
			s_activate_composite (TRUE);
			gldi_dialog_show (D_("Do you want to keep this setting?\nIn 15 seconds, the previous setting will be restored."),
				myIcon, myContainer,
				15e3,
				"same icon",
				NULL,
				(CairoDockActionOnAnswerFunc) _accept_wm_composite,
				g_new0 (gboolean, 1),
				(GFreeFunc)_on_free_wm_dialog);
		}
		else if (myData.bFirstLaunch)
		{
			cd_help_show_welcome_message ();
		}
	}
	else  // just notify him about the problem and its solution.
	{
		gldi_dialog_show (D_("To remove the black rectangle around the dock, you will need to activate a composite manager.\nFor instance, this can be done by activating desktop effects, launching Compiz, or activating the composition in Metacity.\nIf your machine can't support composition, Cairo-Dock can emulate it. This option is in the 'System' module of the configuration, at the bottom of the page."),
			myIcon, myContainer,
			0,
			"same icon",
			NULL,
			NULL,
			NULL,
			(GFreeFunc)_on_free_info_dialog);
	}
	g_free (cPsef);
}

static gboolean cd_help_check_composite (G_GNUC_UNUSED gpointer data)
{
	GdkScreen *pScreen = gdk_screen_get_default ();
	if (! gdk_screen_is_composited (pScreen))  // no composite yet.
	{
		cd_debug ("no composite (%d)", myData.iNbTestComposite);
		myData.iNbTestComposite ++;
		if (myData.iNbTestComposite < 4)  // check during 4 seconds.
			return TRUE;
		
		cd_help_enable_composite ();
	}
	else  // composite is active, but we don't remember this parameter, since it could change if another session is used one day.
	{
		if (myData.bFirstLaunch)
		{
			cd_help_show_welcome_message ();
		}
	}
	
	// remember if we don't need to check the composite any more.
	if (! myData.bTestComposite)
	{
		gchar *cConfFilePath = g_strdup_printf ("%s/.help", g_cCairoDockDataDir);
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_BOOLEAN, "Launch", "test composite", myData.bTestComposite,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
	myData.iSidTestComposite = 0;
	return FALSE;
}

gboolean cd_help_get_params (G_GNUC_UNUSED gpointer data)
{
	// read our file.
	gchar *cConfFilePath = g_strdup_printf ("%s/.help", g_cCairoDockDataDir);
	if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // file already exists, get the latest state.
	{
		GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
		if (pKeyFile != NULL)
		{
			myData.iLastTipGroup = g_key_file_get_integer (pKeyFile, "Last Tip", "group", NULL);
			myData.iLastTipKey = g_key_file_get_integer (pKeyFile, "Last Tip", "key", NULL);
			myData.bTestComposite = g_key_file_get_boolean (pKeyFile, "Launch", "test composite", NULL);
			
			g_key_file_free (pKeyFile);
		}
	}
	else  // no file, means it's the first time we are launched.
	{
		myData.bFirstLaunch = TRUE;
		myData.bTestComposite = TRUE;
		
		// create the file with all the entries
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_BOOLEAN, "Launch", "test composite", myData.bTestComposite,
			G_TYPE_INT, "Last Tip", "group", myData.iLastTipGroup,
			G_TYPE_INT, "Last Tip", "key", myData.iLastTipKey,
			G_TYPE_INVALID);
	}
	
	// test the composite in a few seconds (the Composite Manager may not be active yet).
	if (myData.bTestComposite/** && ! myContainersParam.bUseFakeTransparency*/)
	{
		myData.iSidTestComposite = g_timeout_add_seconds (2, cd_help_check_composite, NULL);
	}
	else if (myData.bFirstLaunch)
	{
		cd_help_show_welcome_message ();
	}
	
	g_free (cConfFilePath);
	myData.iSidGetParams = 0;
	return FALSE;
}
