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

#ifndef __CAIRO_DOCK_OBJECT__
#define  __CAIRO_DOCK_OBJECT__

#include <glib.h>
#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-object.h This class defines the Objects, a sort of GObject, but simpler and more optimized.
*/

struct _GldiObject {
	gint ref;
	GPtrArray *pNotificationsTab;
	GldiManager *mgr;
};

/// signals
typedef enum {
	/// notification called when the object is going to be destroyed. data : NULL
	NOTIFICATION_DESTROY,
	NB_NOTIFICATIONS_OBJECT
	} CairoObjectNotifications;

#define GLDI_OBJECT(p) ((GldiObject*)(p))

#define gldi_object_new(Type, mgr) \
__extension__ ({ \
	Type *obj = g_new0 (Type, 1); \
	GLDI_OBJECT(obj)->ref = 1; \
	gldi_object_set_manager (GLDI_OBJECT (obj), GLDI_MANAGER (mgr)); \
	obj; })

void gldi_object_set_manager (GldiObject *pObject, GldiManager *pMgr);

G_END_DECLS
#endif
