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

#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_create_surface_from_class
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-class-icon-manager.h"

// public (manager, config, data)
GldiObjectManager myClassIconObjectMgr;

// dependancies

// private


static void _load_image (Icon *icon)
{
	int iWidth = icon->iAllocatedWidth;
	int iHeight = icon->iAllocatedWidth;
	cairo_surface_t *pSurface = NULL;
	
	if (icon->pSubDock != NULL && !myIndicatorsParam.bUseClassIndic)  // icone de sous-dock avec un rendu specifique, on le redessinera lorsque les icones du sous-dock auront ete chargees.
	{
		pSurface = cairo_dock_create_blank_surface (iWidth, iHeight);
	}
	else
	{
		cd_debug ("%s (%dx%d)", __func__, iWidth, iHeight);
		pSurface = cairo_dock_create_surface_from_class (icon->cClass,
			iWidth,
			iHeight);  /// could make a gldi_image_buffer_load_from_class...
		if (pSurface == NULL)  // aucun inhibiteur ou aucune image correspondant a cette classe, on cherche a copier une des icones d'appli de cette classe.
		{
			const GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
			if (pApplis != NULL)
			{
				Icon *pOneIcon = (Icon *) (g_list_last ((GList*)pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raisons : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
				cd_debug ("  load from %s (%dx%d)", pOneIcon->cName, iWidth, iHeight);
				pSurface = cairo_dock_duplicate_surface (pOneIcon->image.pSurface,
					pOneIcon->image.iWidth,
					pOneIcon->image.iHeight,
					iWidth,
					iHeight);  /// could make a gldi_image_buffer_load_from_buffer (&pOneIcon->image, iWidth, iHeight) and duplicate the texture only...
			}
		}
	}
	
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}

Icon *gldi_class_icon_new (Icon *pAppliIcon, CairoDock *pClassSubDock)
{
	GldiClassIconAttr attr;
	memset (&attr, 0, sizeof (attr));
	attr.pAppliIcon = pAppliIcon;
	attr.pClassSubDock = pClassSubDock;
	return (Icon*)gldi_object_new (&myClassIconObjectMgr, &attr);
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *pIcon = (Icon*)obj;
	GldiClassIconAttr *pAttributes = (GldiClassIconAttr*)attr;
	Icon *pSameClassIcon = pAttributes->pAppliIcon;
	
	pIcon->iface.load_image = _load_image;
	pIcon->iGroup = pSameClassIcon->iGroup;
	
	const gchar *cClassName = cairo_dock_get_class_name(pSameClassIcon->cClass);
	pIcon->cName = g_strdup (cClassName ? cClassName : pSameClassIcon->cClass);
	if (pSameClassIcon->pAppInfo)
	{
		pIcon->pAppInfo = pSameClassIcon->pAppInfo;
		gldi_object_ref (GLDI_OBJECT (pIcon->pAppInfo));
	}
	pIcon->cClass = g_strdup (pSameClassIcon->cClass);
	pIcon->fOrder = pSameClassIcon->fOrder;
	pIcon->bHasIndicator = pSameClassIcon->bHasIndicator;
	
	pIcon->pSubDock = pAttributes->pClassSubDock;
}

void gldi_register_class_icons_manager (void)
{
	// Manager
	memset (&myClassIconObjectMgr, 0, sizeof (GldiObjectManager));
	myClassIconObjectMgr.cName          = "ClassIcon";
	myClassIconObjectMgr.iObjectSize    = sizeof (GldiClassIcon);
	// interface
	myClassIconObjectMgr.init_object    = init_object;
	myClassIconObjectMgr.reset_object   = NULL;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myClassIconObjectMgr), NB_NOTIFICATIONS_CLASS_ICON);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myClassIconObjectMgr), &myIconObjectMgr);
}
