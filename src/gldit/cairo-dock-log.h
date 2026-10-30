/*
* cairo-dock-log.h
* This file is a part of the Cairo-Dock project
* Login : <ctaf42@gmail.Com>
* Started on  Sat Feb  9 16:11:48 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES
*
* Copyright : (C) 2008 Cedric GESTES
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

#ifndef   	CAIRO_DOCK_LOG_H_
# define   	CAIRO_DOCK_LOG_H_

# include <glib.h>
G_BEGIN_DECLS

/**
 * Internal logging function, called by the cd_warning() and similar macros below.
 */
void cd_log_location(const GLogLevelFlags loglevel,
                     const char *file,
                     const char *func,
                     const int line,
                     const char *format,
                     ...);

/**
 * Internal logging function, called by the cd_return_if_fail() and
 * cd_return_val_if_fail(). This is a separate function from cd_log_location()
 * to make setting breakpoints easier.
 */
void cd_warn_on_fail(const char *file,
                     const char *func,
                     const int line,
                     const char *cond);


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

/**
 * Force the use of colors in the log messages even if these messages are not displayed into a tty.
 */
void cd_log_force_use_color (void);


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

/* Write a debug message on the terminal. Debug message are only useful for developers.
*@param ... the message format and parameters, in a 'printf' style.
*/
#define cd_debug(...)                                                  \
  cd_log_location(G_LOG_LEVEL_DEBUG, __FILE__, __PRETTY_FUNCTION__, __LINE__,__VA_ARGS__)


/**
 * Print a warning to the terminal and return from the current function if the given
 * condition does not evaluate to TRUE. This is similar to g_return_if_fail() in GLib,
 * but with consistent and well-defined behavior, so it can be used for conditions that
 * can possibly be FALSE without UB. Also, the check will not be disabled with
 * G_DISABLE_CHECKS.
 * See also https://gitlab.gnome.org/GNOME/glib/-/work_items/4075 for more context.
 *@param cond the condition to test
 */
#define cd_return_if_fail(cond) do { \
	if (G_UNLIKELY (! (cond))) { \
		cd_warn_on_fail(__FILE__, __PRETTY_FUNCTION__, __LINE__, #cond); \
		return; \
	} \
} while (0)

/**
 * Print a warning to the terminal and return the given value from the current function
 * if the condition does not evaluate to TRUE. This is similar to g_return_val_if_fail().
 *@param cond the condition to test
 *@param val the value to return (must be compatible with the current function's return value)
 */
#define cd_return_val_if_fail(cond, val) do { \
	if (G_UNLIKELY (! (cond))) { \
		cd_warn_on_fail(__FILE__, __PRETTY_FUNCTION__, __LINE__, #cond); \
		return (val); \
	} \
} while (0)

G_END_DECLS
#endif 	    /* !CAIRO_DOCK_LOG_H_ */
