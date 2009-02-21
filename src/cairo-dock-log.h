/*
** cairo-dock-log.h
** Login : <ctaf42@gmail.Com>
** Started on  Sat Feb  9 16:11:48 2008 Cedric GESTES
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

#ifndef   	CAIRO_DOCK_LOG_H_
# define   	CAIRO_DOCK_LOG_H_

# include <gtk/gtk.h>
G_BEGIN_DECLS

# ifndef _INSIDE_CAIRO_DOCK_LOG_C_
  extern GLogLevelFlags gLogLevel;
# endif

/*
 * internal function
 */
void cd_log_location(const GLogLevelFlags loglevel,
                     const char *file,
                     const char *func,
                     const int line,
                     const char *format,
                     ...);

/**
 * initialise the log system
 */
void cd_log_init(gboolean bBlackTerminal);

/**
 * set the verbosity level
 */
void cd_log_set_level(GLogLevelFlags loglevel);

#define cd_error(...)                                                  \
  cd_log_location(G_LOG_LEVEL_ERROR, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

#define cd_critical(...)                                               \
  cd_log_location(G_LOG_LEVEL_CRITICAL, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

#define cd_warning(...)                                                \
  cd_log_location(G_LOG_LEVEL_WARNING, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

#define cd_message(...)                                                \
  cd_log_location(G_LOG_LEVEL_MESSAGE, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

#define cd_debug(...)                                                  \
  cd_log_location(G_LOG_LEVEL_DEBUG, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

G_END_DECLS
#endif 	    /* !CAIRO_DOCK_LOG_H_ */
