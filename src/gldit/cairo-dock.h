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

#ifndef __CAIRO_DOCK_H__
#define __CAIRO_DOCK_H__


#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <gldit/cairo-dock-struct.h>
#include <gldit/gldi-config.h>
#include <gldit/cairo-dock-global-variables.h>
#include <gldit/cairo-dock-core.h>
// icon factory
#include <icon-factory/cairo-dock-application-factory.h>
#include <icon-factory/cairo-dock-applet-factory.h>
#include <icon-factory/cairo-dock-launcher-factory.h>
#include <icon-factory/cairo-dock-separator-factory.h>
#include <icon-factory/cairo-dock-desktop-file-factory.h>
// structures de base.
#include <gldit/cairo-dock-module-factory.h>
#include <gldit/cairo-dock-icon-factory.h>
#include <gldit/cairo-dock-container.h>
#include <gldit/cairo-dock-flying-container.h>
#include <gldit/cairo-dock-dialog-factory.h>
#include <gldit/cairo-dock-desklet-factory.h>
#include <gldit/cairo-dock-dock-factory.h>
#include <gldit/cairo-dock-callbacks.h>
// dessin
#include <gldit/cairo-dock-opengl.h>
#include <gldit/cairo-dock-opengl-path.h>
#include <gldit/cairo-dock-opengl-font.h>
#include <gldit/cairo-dock-draw-opengl.h>
#include <gldit/cairo-dock-draw.h>
///#include <gldit/cairo-dock-emblem.h>
#include <gldit/cairo-dock-overlay.h>
#include <gldit/cairo-dock-dock-facility.h>
#include <gldit/cairo-dock-animations.h>
// chargeurs d'icone.
#include <gldit/cairo-dock-icon-manager.h>
#include <gldit/cairo-dock-applications-manager.h>
#include <gldit/cairo-dock-applet-manager.h>
#include <gldit/cairo-dock-launcher-manager.h>
#include <gldit/cairo-dock-separator-manager.h>
#include <gldit/cairo-dock-application-facility.h>
#include <gldit/cairo-dock-dialog-manager.h>
#include <gldit/cairo-dock-image-buffer.h>
#include <gldit/cairo-dock-icon-facility.h>
#include <gldit/cairo-dock-config.h>
// managers.
#include <gldit/cairo-dock-manager.h>
#include <gldit/cairo-dock-module-manager.h>
#include <gldit/cairo-dock-X-manager.h>
#include <gldit/cairo-dock-indicator-manager.h>
#include <gldit/cairo-dock-applications-manager.h>
#include <gldit/cairo-dock-class-manager.h>
#include <gldit/cairo-dock-dock-manager.h>
#include <gldit/cairo-dock-desklet-manager.h>
#include <gldit/cairo-dock-backends-manager.h>
#include <gldit/cairo-dock-file-manager.h>
// GUI.
#include <gldit/cairo-dock-themes-manager.h>
#include <gldit/cairo-dock-gui-manager.h>
#include <gldit/cairo-dock-gui-factory.h>
// architecture pour les applets.
#include <gldit/cairo-dock-applet-facility.h>
#include <gldit/cairo-dock-applet-canvas.h>
// classes presque independantes de CD.
#include <gldit/cairo-dock-packages.h>
#include <implementations/cairo-dock-progressbar.h>
#include <implementations/cairo-dock-graph.h>
#include <implementations/cairo-dock-gauge.h>
#include <gldit/cairo-dock-notifications.h>
#include <gldit/cairo-dock-surface-factory.h>
#include <gldit/cairo-dock-X-utilities.h>
// classes independantes de CD.
#include <gldit/cairo-dock-log.h>
#include <gldit/cairo-dock-dbus.h>
#include <gldit/cairo-dock-keyfile-utilities.h>
#include <gldit/cairo-dock-keybinder.h>
#include <gldit/cairo-dock-task.h>
#include <gldit/cairo-dock-particle-system.h>

#endif
