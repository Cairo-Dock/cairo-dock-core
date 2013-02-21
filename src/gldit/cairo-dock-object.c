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

#include "cairo-dock-struct.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-manager.h"
#include "cairo-dock-object.h"

/* new_dialog -> new_object (mgr, attr) -> mgr->top_parent->init(attr) -> mgr->parent->init(attr) -> mgr->init(attr) --> notif
 * unref_object -> ref-- -> notif -> mgr->destroy -> mgr->parent->destroy -> mgr->top_parent->destroy --> free
 * */

void gldi_object_set_manager (GldiObject *pObject, GldiManager *pMgr)
{
	pObject->mgr = pMgr;
	cairo_dock_install_notifications_on_object (pObject, pMgr->object.pNotificationsTab->len);
}
