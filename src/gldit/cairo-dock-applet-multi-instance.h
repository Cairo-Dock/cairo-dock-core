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


#ifndef __CAIRO_DOCK_APPLET_MULTI_INSTANCE__
#define  __CAIRO_DOCK_APPLET_MULTI_INSTANCE__

/**
*@file cairo-dock-applet-multi-intance.h This file defines macros specific to multi-instance applets.
* The same are also defined for single-instance applets with the same interface.
*/


#define CD_APPLET_DEFINE_BEGIN(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
	CD_APPLET_DEFINE_ALL_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
	pVisitCard->bMultiInstance = TRUE;

/** General applet definition. Use this to define an applet and its interface in more complex cases.
 * Code enclosed between this macro and \ref CD_APPLET_DEFINE2_END will be executed when the plug-in
 * module is first loaded and can perform basic checks and initialization that must be done before the
 * module is activated via the \ref CD_APPLET_INIT_BEGIN function. Be careful that the applet's instance
 * variable (myApplet) and configuration (myConfig) are not available at this point. You can call the
 * \ref gldi_module_disable () function from here to disable the module entirely if it is not possible
 * to use it in the current environment.
*@param cName name of the applet, shown to the user (use the N_() macro to translate it)
*@param iFlags flags describing module compatibility; see \ref GldiModuleFlags
*@param iAppletCategory category of the applet; see \ref GldiModuleCategory for available categories
*@param cDescription (short) description of the applet, to be displayed to the user in a dialog bubble when the mouse is hovered on this applet in the configuration window
*@param cAuthor Applet creator, displayed on the applet's settings page
*/
#define CD_APPLET_DEFINE2_BEGIN(cName, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
	CD_APPLET_DEFINE2_ALL_BEGIN (cName, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
	pVisitCard->bMultiInstance = TRUE;


#define CD_APPLET_INIT_BEGIN CD_APPLET_INIT_ALL_BEGIN (myApplet)

#define myIcon myApplet->pIcon
#define myContainer myApplet->pContainer
#define myDock myApplet->pDock
#define myDesklet myApplet->pDesklet
#define myDrawContext myApplet->pDrawContext
#define myConfigPtr ((AppletConfig *)myApplet->pConfig)
#define myConfig (*myConfigPtr)
#define myDataPtr ((AppletData *)myApplet->pData)
#define myData (*myDataPtr)

#define CD_APPLET_RELOAD_BEGIN CD_APPLET_RELOAD_ALL_BEGIN

#define CD_APPLET_RESET_DATA_END CD_APPLET_RESET_DATA_ALL_END

#define CD_APPLET_RESET_CONFIG_BEGIN CD_APPLET_RESET_CONFIG_ALL_BEGIN

#define CD_APPLET_RESET_CONFIG_END CD_APPLET_RESET_CONFIG_ALL_END

#define CD_APPLET_GET_CONFIG_BEGIN CD_APPLET_GET_CONFIG_ALL_BEGIN


#endif
