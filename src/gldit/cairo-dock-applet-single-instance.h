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


#ifndef __CAIRO_DOCK_APPLET_SINGLE_INSTANCE__
#define  __CAIRO_DOCK_APPLET_SINGLE_INSTANCE__

#define myDrawContext myApplet->pDrawContext

#define CD_APPLET_SINGLE_INSTANCE_STATIC_VARS \
GldiModuleInstance *myApplet = NULL; \
Icon *myIcon; \
GldiContainer *myContainer; \
CairoDock *myDock; \
CairoDesklet *myDesklet; \
AppletConfig *myConfigPtr = NULL; \
AppletData *myDataPtr = NULL;

#define CD_APPLET_DEFINE_BEGIN(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_SINGLE_INSTANCE_STATIC_VARS \
CD_APPLET_DEFINE_ALL_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
pVisitCard->bMultiInstance = FALSE;

#define CD_APPLET_DEFINE2_BEGIN(cName, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
CD_APPLET_SINGLE_INSTANCE_STATIC_VARS \
CD_APPLET_DEFINE2_ALL_BEGIN (cName, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
pVisitCard->bMultiInstance = FALSE;

#define CD_APPLET_INIT_BEGIN \
CD_APPLET_INIT_ALL_BEGIN(pApplet) \
myApplet = pApplet; \
myIcon = myApplet->pIcon; \
myContainer = myApplet->pContainer; \
myDock = myApplet->pDock; \
myDesklet = myApplet->pDesklet;\
myDataPtr = (AppletData*)myApplet->pData;

#define myConfig (* myConfigPtr)
#define myData (* myDataPtr)


#define CD_APPLET_RELOAD_BEGIN \
	CD_APPLET_RELOAD_ALL_BEGIN \
	myContainer = myApplet->pContainer; \
	myDock = myApplet->pDock; \
	myDesklet = myApplet->pDesklet; \


#define CD_APPLET_RESET_DATA_END \
	myDock = NULL; \
	myContainer = NULL; \
	myIcon = NULL; \
	myDataPtr = NULL; \
	myDesklet = NULL; \
	myApplet = NULL; \
	CD_APPLET_RESET_DATA_ALL_END


#define CD_APPLET_RESET_CONFIG_BEGIN \
	CD_APPLET_RESET_CONFIG_ALL_BEGIN \
	if (myConfigPtr == NULL) \
		return ;

#define CD_APPLET_RESET_CONFIG_END \
	myConfigPtr = NULL; \
	CD_APPLET_RESET_CONFIG_ALL_END

#define CD_APPLET_GET_CONFIG_BEGIN \
	CD_APPLET_GET_CONFIG_ALL_BEGIN\
	if (myConfigPtr == NULL)\
		myConfigPtr = (AppletConfig*)myApplet->pConfig;\
	if (myDataPtr == NULL)\
		myDataPtr = (AppletData*)myApplet->pData;


extern Icon *myIcon;
extern GldiContainer *myContainer;
extern CairoDock *myDock;
extern CairoDesklet *myDesklet;
extern AppletConfig *myConfigPtr;
extern AppletData *myDataPtr;
extern GldiModuleInstance *myApplet;

#endif
