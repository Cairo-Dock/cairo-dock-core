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

#include "cairo-dock-log.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-widget.h"


void cairo_dock_widget_apply (CDWidget *pCdWidget)
{
	if (pCdWidget && pCdWidget->apply)
		pCdWidget->apply (pCdWidget);
}


gboolean cairo_dock_widget_can_apply (CDWidget *pCdWidget)
{
	return (pCdWidget && pCdWidget->apply);
}


void cairo_dock_widget_reload (CDWidget *pCdWidget)
{
	cd_debug ("%s (type %d)", __func__, pCdWidget->iType);
	if (pCdWidget && pCdWidget->reload)
		pCdWidget->reload (pCdWidget);
}


void cairo_dock_widget_free (CDWidget *pCdWidget)  // doesn't destroy the GTK widget, as it is destroyed with the main window
{
	if (!pCdWidget)
		return;
	if (pCdWidget->reset)
		pCdWidget->reset (pCdWidget);
	
	cairo_dock_free_generated_widget_list (pCdWidget->pWidgetList);
	pCdWidget->pWidgetList = NULL;
	
	g_ptr_array_free (pCdWidget->pDataGarbage, TRUE);  /// free each element...
	pCdWidget->pDataGarbage = NULL;
	
	g_free (pCdWidget);
}


void cairo_dock_widget_destroy_widget (CDWidget *pCdWidget)
{
	if (!pCdWidget)
		return;
	
	gtk_widget_destroy (pCdWidget->pWidget);
	pCdWidget->pWidget = NULL;
	
	cairo_dock_free_generated_widget_list (pCdWidget->pWidgetList);
	pCdWidget->pWidgetList = NULL;
	
	/// free each element...
	g_ptr_array_free (pCdWidget->pDataGarbage, TRUE);
	pCdWidget->pDataGarbage = NULL;
}
