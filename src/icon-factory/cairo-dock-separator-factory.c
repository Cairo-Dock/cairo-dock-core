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

#include <math.h>
#include <string.h>
#include <cairo.h>
#include <stdlib.h>


#include "cairo-dock-icon-factory.h"
#include "cairo-dock-separator-factory.h"


Icon *cairo_dock_new_separator_icon (int iSeparatorType)
{
	//\____________ On cree l'icone.
	Icon *icon = cairo_dock_new_icon ();
	icon->iTrueType = CAIRO_DOCK_ICON_TYPE_SEPARATOR;
	icon->iGroup = iSeparatorType;
	
	return icon;
}
