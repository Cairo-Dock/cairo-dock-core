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


#ifndef __CAIRO_FLYING_CONTAINER__
#define  __CAIRO_FLYING_CONTAINER__

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS

// manager
typedef struct _CairoFlyingManager CairoFlyingManager;
typedef struct _CairoFlyingAttr CairoFlyingAttr;

#ifndef _MANAGER_DEF_
extern CairoFlyingManager myFlyingsMgr;
extern GldiObjectManager myFlyingObjectMgr;
#endif

struct _CairoFlyingManager {
	GldiManager mgr;
	} ;

struct _CairoFlyingAttr {
	GldiContainerAttr cattr;
	Icon *pIcon;
	CairoDock *pOriginDock;
	} ;

// signals
typedef enum {
	NB_NOTIFICATIONS_FLYING_CONTAINER = NB_NOTIFICATIONS_CONTAINER
	} CairoFlyingNotifications;

// factory
struct _CairoFlyingContainer {
	/// container
	GldiContainer container;
	/// the flying icon
	Icon *pIcon;
	/// time the container was created.
	double fCreationTime;  // see callbacks.c for the usage of this.
};

/** Cast a Container into a FlyingContainer .
*@param pContainer the container.
*@return the FlyingContainer.
*/
#define CAIRO_FLYING_CONTAINER(pContainer) ((CairoFlyingContainer *)pContainer)

/** Say if an object is a FlyingContainer.
*@param obj the object.
*@return TRUE if the object is a FlyingContainer.
*/
#define CAIRO_DOCK_IS_FLYING_CONTAINER(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myFlyingObjectMgr)


CairoFlyingContainer *gldi_flying_container_new (Icon *pFlyingIcon, CairoDock *pOriginDock);

void gldi_flying_container_drag (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock);

void gldi_flying_container_terminate (CairoFlyingContainer *pFlyingContainer);


void gldi_register_flying_manager (void);


G_END_DECLS
#endif
