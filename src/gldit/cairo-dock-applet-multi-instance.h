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


#define CD_APPLET_DEFINE_BEGIN(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
	CD_APPLET_DEFINE_ALL_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
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
