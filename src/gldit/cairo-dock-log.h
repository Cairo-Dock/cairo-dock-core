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

# include <glib.h>
G_BEGIN_DECLS

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
 * Initialize the log system.
 */
void cd_log_init(gboolean bBlackTerminal);

/**
 * Set the verbosity level.
 */
void cd_log_set_level(GLogLevelFlags loglevel);

/**
 * Set the verbosity level from a readable verbosity.
 */
void cd_log_set_level_from_name (const gchar *cVerbosity);


/* Write an error message on the terminal. Error messages are used to indicate the cause of the program stop.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_error(...)                                                  \
  cd_log_location(G_LOG_LEVEL_ERROR, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

/* Write a critical message on the terminal. Critical messages should be as clear as possible to be useful for end-users.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_critical(...)                                               \
  cd_log_location(G_LOG_LEVEL_CRITICAL, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

/* Write a warning message on the terminal. Warnings should be as clear as possible to be useful for end-users.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_warning(...)                                                \
  cd_log_location(G_LOG_LEVEL_WARNING, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

/* Write a message on the terminal. Messages are used to trace the sequence of functions, and may be used by users for a quick debug.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_message(...)                                                \
  cd_log_location(G_LOG_LEVEL_MESSAGE, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

/* Write a debug message on the terminal. Debug message are only useful for developpers.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_debug(...)                                                  \
  cd_log_location(G_LOG_LEVEL_DEBUG, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)

G_END_DECLS
#endif 	    /* !CAIRO_DOCK_LOG_H_ */
