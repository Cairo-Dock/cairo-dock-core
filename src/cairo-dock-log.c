/*
** cairo-dock-log.c
** Login : <ctaf42@gmail.com>
** Started on  Sat Feb  9 15:54:57 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#include <stdio.h>
#include <stdarg.h>
#include <glib.h>

#define _INSIDE_CAIRO_DOCK_LOG_C_

#include "cairo-dock-log.h"

char s_iLogColor = '0';
static GLogLevelFlags gLogLevel = 0;

/* #    'default'     => "\033[1m", */

/* #    'black'     => "\033[30m", */
/* #    'red'       => "\033[31m", */
/* #    'green'     => "\033[32m", */
/* #    'yellow'    => "\033[33m", */
/* #    'blue'      => "\033[34m", */
/* #    'magenta'   => "\033[35m", */
/* #    'cyan'      => "\033[36m", */
/* #    'white'     => "\033[37m", */


const char*_cd_log_level_to_string(const GLogLevelFlags loglevel)
{
  switch(loglevel)
  {
  case G_LOG_LEVEL_CRITICAL:
    return "\033[1;31mCRITICAL: \033[0m ";
  case G_LOG_LEVEL_ERROR:
    return "\033[1;31mERROR   : \033[0m ";
  case G_LOG_LEVEL_WARNING:
    return "\033[1;38mwarning : \033[0m ";
  case G_LOG_LEVEL_MESSAGE:
    return "\033[1;32mmessage : \033[0m ";
  case G_LOG_LEVEL_INFO:
    return "\033[1;33minfo    : \033[0m ";
  case G_LOG_LEVEL_DEBUG:
    return "\033[1;35mdebug   : \033[0m ";
  }
  return "";
}

void cd_log_location(const GLogLevelFlags loglevel,
                     const char *file,
                     const char *func,
                     const int line,
                     const char *format,
                     ...)
{
  va_list args;

  if (loglevel > gLogLevel)
    return;
  g_print(_cd_log_level_to_string(loglevel));
  g_print("\033[0;37m(%s:%s:%d) \033[%cm \n  ", file, func, line, s_iLogColor);
  va_start(args, format);
  g_logv(G_LOG_DOMAIN, loglevel, format, args);
  va_end(args);
}

static void cairo_dock_log_handler(const gchar *log_domain,
                                   GLogLevelFlags log_level,
                                   const gchar *message,
                                   gpointer user_data)
{
  if (log_level > gLogLevel)
    return;
  g_print("%s\n", message);
}

void cd_log_init(gboolean bBlackTerminal)
{
  g_log_set_default_handler(cairo_dock_log_handler, NULL);
  s_iLogColor = (bBlackTerminal ? '1' : '0');
}

void cd_log_set_level(GLogLevelFlags loglevel)
{
  gLogLevel = loglevel;
}
