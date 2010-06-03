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

/** /mainpage ceci est la mainpage
* /subsection ceci est une sous-section 1
* /subsection ceci est une sous-section 2
*/

#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <cairo-dock/cairo-dock-struct.h>
#include <cairo-dock/cairo-dock-global-variables.h>
// icon factory
#include <icon-factory/cairo-dock-application-factory.h>
#include <icon-factory/cairo-dock-applet-factory.h>
#include <icon-factory/cairo-dock-launcher-factory.h>
#include <icon-factory/cairo-dock-separator-factory.h>
#include <icon-factory/cairo-dock-desktop-file-factory.h>
// structures de base.
#include <cairo-dock/cairo-dock-icons.h>
#include <cairo-dock/cairo-dock-container.h>
#include <cairo-dock/cairo-dock-modules.h>
#include <cairo-dock/cairo-dock-flying-container.h>
#include <cairo-dock/cairo-dock-dialog-factory.h>
#include <cairo-dock/cairo-dock-desklet-factory.h>
#include <cairo-dock/cairo-dock-dock-factory.h>
#include <cairo-dock/cairo-dock-callbacks.h>
// dessin
#include <cairo-dock/cairo-dock-opengl.h>
#include <cairo-dock/cairo-dock-opengl-path.h>
#include <cairo-dock/cairo-dock-opengl-font.h>
#include <cairo-dock/cairo-dock-draw-opengl.h>
#include <cairo-dock/cairo-dock-draw.h>
#include <cairo-dock/cairo-dock-emblem.h>
#include <cairo-dock/cairo-dock-dock-facility.h>
#include <cairo-dock/cairo-dock-animations.h>
// chargeurs d'icone.
#include <cairo-dock/cairo-dock-applications-manager.h>
#include <cairo-dock/cairo-dock-applet-manager.h>
#include <cairo-dock/cairo-dock-launcher-manager.h>
#include <cairo-dock/cairo-dock-separator-manager.h>
#include <cairo-dock/cairo-dock-application-facility.h>
#include <cairo-dock/cairo-dock-dialog-manager.h>
#include <cairo-dock/cairo-dock-load.h>
#include <cairo-dock/cairo-dock-icon-loader.h>
#include <cairo-dock/cairo-dock-config.h>
// managers.
#include <cairo-dock/cairo-dock-X-manager.h>
#include <cairo-dock/cairo-dock-indicator-manager.h>
#include <cairo-dock/cairo-dock-applications-manager.h>
#include <cairo-dock/cairo-dock-class-manager.h>
#include <cairo-dock/cairo-dock-dock-manager.h>
#include <cairo-dock/cairo-dock-desklet-manager.h>
#include <cairo-dock/cairo-dock-backends-manager.h>
#include <cairo-dock/cairo-dock-file-manager.h>
// GUI.
#include <cairo-dock/cairo-dock-themes-manager.h>
#include <cairo-dock/cairo-dock-gui-manager.h>
#include <cairo-dock/cairo-dock-gui-factory.h>
// donnees et modules internes.
#include <cairo-dock/cairo-dock-internal-position.h>
#include <cairo-dock/cairo-dock-internal-accessibility.h>
#include <cairo-dock/cairo-dock-internal-system.h>
#include <cairo-dock/cairo-dock-internal-taskbar.h>
#include <cairo-dock/cairo-dock-internal-dialogs.h>
#include <cairo-dock/cairo-dock-internal-indicators.h>
#include <cairo-dock/cairo-dock-internal-views.h>
#include <cairo-dock/cairo-dock-internal-desklets.h>
#include <cairo-dock/cairo-dock-internal-labels.h>
#include <cairo-dock/cairo-dock-internal-background.h>
#include <cairo-dock/cairo-dock-internal-icons.h>
// architecture pour les applets.
#include <cairo-dock/cairo-dock-applet-facility.h>
#include <cairo-dock/cairo-dock-applet-canvas.h>
// classes presque independantes de CD.
#include <cairo-dock/cairo-dock-data-renderer.h>
#include <implementations/cairo-dock-graph.h>
#include <implementations/cairo-dock-gauge.h>
#include <cairo-dock/cairo-dock-notifications.h>
#include <cairo-dock/cairo-dock-surface-factory.h>
#include <cairo-dock/cairo-dock-X-utilities.h>
// classes independantes de CD.
#include <cairo-dock/cairo-dock-log.h>
#include <cairo-dock/cairo-dock-dbus.h>
#include <cairo-dock/cairo-dock-keyfile-utilities.h>
#include <cairo-dock/cairo-dock-keybinder.h>
#include <cairo-dock/cairo-dock-task.h>
#include <cairo-dock/cairo-dock-particle-system.h>

#endif
