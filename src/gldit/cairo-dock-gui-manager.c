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

#include <string.h>
#include <unistd.h>
#define __USE_XOPEN_EXTENDED
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "gldi-config.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-container-priv.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-image-buffer.h"
#define _MANAGER_DEF_
#include "cairo-dock-gui-manager.h"

static CairoDockGuiBackend *s_pGuiBackend = NULL;

  /////////////////
 // GUI BACKEND //
/////////////////

void cairo_dock_register_gui_backend (CairoDockGuiBackend *pBackend)
{
	g_free (s_pGuiBackend);
	s_pGuiBackend = pBackend;
}


void cairo_dock_set_status_message (GtkWidget *pWindow, const gchar *cMessage)
{
	cd_debug ("%s (%s)", __func__, cMessage);
	GtkWidget *pStatusBar;
	if (pWindow != NULL)
	{
		pStatusBar = g_object_get_data (G_OBJECT (pWindow), "status-bar");
		if (pStatusBar == NULL)
			return ;
		//g_print ("%s (%s sur %x/%x)\n", __func__, cMessage, pWindow, pStatusBar);
		gtk_statusbar_pop (GTK_STATUSBAR (pStatusBar), 0);  // clear any previous message, underflow is allowed.
		gtk_statusbar_push (GTK_STATUSBAR (pStatusBar), 0, cMessage);
	}
	else
	{
		if (s_pGuiBackend && s_pGuiBackend->set_status_message_on_gui)
			s_pGuiBackend->set_status_message_on_gui (cMessage);
	}
}

void cairo_dock_set_status_message_printf (GtkWidget *pWindow, const gchar *cFormat, ...)
{
	g_return_if_fail (cFormat != NULL);
	va_list args;
	va_start (args, cFormat);
	gchar *cMessage = g_strdup_vprintf (cFormat, args);
	cairo_dock_set_status_message (pWindow, cMessage);
	g_free (cMessage);
	va_end (args);
}

void cairo_dock_reload_current_widget_full (GldiModuleInstance *pModuleInstance, int iShowPage)
{
	if (s_pGuiBackend && s_pGuiBackend->reload_current_widget)
		s_pGuiBackend->reload_current_widget (pModuleInstance, iShowPage);
}

void cairo_dock_show_module_instance_gui (GldiModuleInstance *pModuleInstance, int iShowPage)
{
	if (s_pGuiBackend && s_pGuiBackend->show_module_instance_gui)
		s_pGuiBackend->show_module_instance_gui (pModuleInstance, iShowPage);
}
