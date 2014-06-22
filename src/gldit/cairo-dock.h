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

#include <gldit/cairo-dock-struct.h>
#include <gldit/gldi-config.h>
#include <gldit/cairo-dock-global-variables.h>
#include <gldit/cairo-dock-core.h>
// Containers
#include <gldit/cairo-dock-container.h>
#include <gldit/cairo-dock-dock-manager.h>
#include <gldit/cairo-dock-dock-factory.h>
#include <gldit/cairo-dock-desklet-manager.h>
#include <gldit/cairo-dock-desklet-factory.h>
#include <gldit/cairo-dock-dialog-manager.h>
#include <gldit/cairo-dock-dialog-factory.h>
#include <gldit/cairo-dock-flying-container.h>
#include <gldit/cairo-dock-menu.h>
// Icons
#include <gldit/cairo-dock-icon-manager.h>
#include <gldit/cairo-dock-applications-manager.h>
#include <gldit/cairo-dock-applet-manager.h>
#include <gldit/cairo-dock-launcher-manager.h>
#include <gldit/cairo-dock-separator-manager.h>
#include <gldit/cairo-dock-stack-icon-manager.h>
#include <gldit/cairo-dock-class-icon-manager.h>
#include <gldit/cairo-dock-application-facility.h>
#include <gldit/cairo-dock-icon-facility.h>
#include <gldit/cairo-dock-icon-factory.h>
#include "gldi-icon-names.h"
// managers.
#include <gldit/cairo-dock-module-manager.h>
#include <gldit/cairo-dock-module-instance-manager.h>
#include <gldit/cairo-dock-desktop-manager.h>
#include <gldit/cairo-dock-windows-manager.h>
#include <gldit/cairo-dock-indicator-manager.h>
#include <gldit/cairo-dock-applications-manager.h>
#include <gldit/cairo-dock-class-manager.h>
#include <gldit/cairo-dock-backends-manager.h>
#include <gldit/cairo-dock-file-manager.h>
#include <gldit/cairo-dock-themes-manager.h>
#include <gldit/cairo-dock-config.h>
// drawing
#include <gldit/cairo-dock-opengl.h>
#include <gldit/cairo-dock-opengl-path.h>
#include <gldit/cairo-dock-opengl-font.h>
#include <gldit/cairo-dock-draw-opengl.h>
#include <gldit/cairo-dock-draw.h>
#include <gldit/cairo-dock-overlay.h>
#include <gldit/cairo-dock-dock-facility.h>
#include <gldit/cairo-dock-animations.h>
// GUI
#include <gldit/cairo-dock-gui-manager.h>
#include <gldit/cairo-dock-gui-factory.h>
// used by applets
#include <gldit/cairo-dock-applet-facility.h>
#include <gldit/cairo-dock-applet-canvas.h>
#include <implementations/cairo-dock-progressbar.h>
#include <implementations/cairo-dock-graph.h>
#include <implementations/cairo-dock-gauge.h>
// base classes
#include <gldit/cairo-dock-object.h>
#include <gldit/cairo-dock-manager.h>
#include <gldit/cairo-dock-log.h>
#include <gldit/cairo-dock-dbus.h>
#include <gldit/cairo-dock-keyfile-utilities.h>
#include <gldit/cairo-dock-keybinder.h>
#include <gldit/cairo-dock-task.h>
#include <gldit/cairo-dock-particle-system.h>
#include <gldit/cairo-dock-packages.h>
#include <gldit/cairo-dock-surface-factory.h>
#include <gldit/cairo-dock-image-buffer.h>
#include <gldit/cairo-dock-style-facility.h>
#include <gldit/cairo-dock-style-manager.h>

#include <gldit/cairo-dock-utils.h>

#endif
