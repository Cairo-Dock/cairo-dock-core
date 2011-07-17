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

#include "cairo-dock-applet-canvas.h"
#include "applet-struct.h"
#include "applet-init.h"
//#include "applet-config.h"
#include "cairo-dock-help.h"


void cairo_dock_register_help_module (void)
{
	CairoDockVisitCard *pVisitCard = g_new0 (CairoDockVisitCard, 1);
	CairoDockModuleInterface *pInterface = g_new0 (CairoDockModuleInterface, 1);
	CD_APPLET_DEFINE_FUNC (pVisitCard, pInterface);
	/**
	// define the applet properties
	CairoDockVisitCard *pVisitCard = g_new0 (CairoDockVisitCard, 1);
	pVisitCard->cModuleName = "Help";
	pVisitCard->cTitle = _("Help");
	pVisitCard->iMajorVersionNeeded = 2;
	pVisitCard->iMinorVersionNeeded = 4;
	pVisitCard->iMicroVersionNeeded = 0;
	pVisitCard->cPreviewFilePath = NULL;  /// add a preview...
	pVisitCard->cGettextDomain = NULL;  // same as the dock.
	pVisitCard->cDockVersionOnCompilation = CAIRO_DOCK_VERSION;
	pVisitCard->cUserDataDir = "Help";
	pVisitCard->cShareDataDir = CAIRO_DOCK_SHARE_DATA_DIR;
	pVisitCard->cConfFileName = "Help.conf";
	pVisitCard->cModuleVersion = "0.2.0";
	pVisitCard->iCategory = CAIRO_DOCK_CATEGORY_BEHAVIOR;
	pVisitCard->cIconFilePath = CAIRO_DOCK_SHARE_DATA_DIR"/icons/icon-help.svg";
	pVisitCard->iSizeOfConfig = sizeof (AppletConfig);
	pVisitCard->iSizeOfData = sizeof (AppletData);
	pVisitCard->cDescription = N_("This applet is made to help you.\n"
		"Click on its icon to pop up useful tips about the possibilities of Cairo-Dock.\n"
		"Middle-click to open the configuration window.\n"
		"Right-click to access some troubleshooting actions.");
	pVisitCard->cAuthor = "Fabounet (Fabrice Rey)";
	// define its interface
	CairoDockModuleInterface *pInterface = g_new0 (CairoDockModuleInterface, 1);
	CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE
	*/
	// finally, register it.
	CairoDockModule *pHelpModule = g_new0 (CairoDockModule, 1);
	pHelpModule->pVisitCard = pVisitCard;
	pHelpModule->pInterface = pInterface;
	cairo_dock_register_module (pHelpModule);
}
