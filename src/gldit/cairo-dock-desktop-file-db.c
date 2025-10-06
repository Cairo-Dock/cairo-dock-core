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


#include <glib.h>
#include <string.h>
#include <gio/gio.h>
#include <gmodule.h>
#include "cairo-dock-desktop-file-db.h"
#include "cairo-dock-class-manager-priv.h" // cairo_dock_guess_class
#include "cairo-dock-log.h" // cd_error

static GAppInfoMonitor *monitor = NULL;

typedef struct _desktop_db {
	// hash tables store GDesktopAppInfo
	GHashTable *class_table; // main table mapping desktop file IDs (without the extension)
	GHashTable *alt_class_table; // alternative mapping based on the StartupWMClass (if exists)
} desktop_db;

static desktop_db *db_current = NULL; // current database (used for lookups)
static desktop_db *db_pending = NULL; // pending database (created by our worker thread)

static GMutex mutex; // mutex for accessing db_pending
static GCond cond; // condition to signal that db_pending has been updated (only used if db_current == NULL)
static GThread *thread = NULL; // our worker thread

static gboolean update_pending = FALSE; // update of apps is already pending
static gboolean more_work = FALSE; // more work to do for the worker thread (apps on the system have changed)
static gboolean thread_running = FALSE; // worker thread is running (doing work updating the app DB; set to TRUE in _start_thread())
static gboolean error = FALSE; // set if the worker cannot retrieve apps

static void _desktop_db_free (desktop_db *db)
{
	if (db)
	{
		if (db->alt_class_table) g_hash_table_unref(db->alt_class_table);
		if (db->class_table) g_hash_table_unref(db->class_table);
		g_free (db);
	}
}

static void _process_app (gpointer data, gpointer user_data)
{
	if (!data) return;
	desktop_db *db = (desktop_db*)user_data;
	GAppInfo *app = (GAppInfo*)data;
	const char *id = g_app_info_get_id (app);
	if (!id) return;
	
	GDesktopAppInfo *desktop_app = G_DESKTOP_APP_INFO(app);
	if (!desktop_app) return;
	
	const char *fn = g_desktop_app_info_get_filename (desktop_app);
	if (!fn) return;
	
	const char *wmclass = g_desktop_app_info_get_startup_wm_class (desktop_app);
	const char *cmdline = g_app_info_get_commandline (app);
	
	// process ID: make it lowercase and remove .desktop extension
	char *id_lower = NULL;
	const char *tmp = strrchr (id, '.');
	if (tmp && !strcmp(tmp, ".desktop"))
		id_lower = g_ascii_strdown (id, tmp - id);
	else id_lower = g_ascii_strdown (id, -1);
	
	// check if this ID exists (desktop file names should be unique, so there is no use adding in that case)
	if (g_hash_table_contains (db->class_table, id_lower))
	{
		g_free (id_lower);
		return;
	}
	
	// add the app ID to the (main) hash table
	g_object_ref (desktop_app);
	g_hash_table_insert (db->class_table, id_lower, desktop_app);
	
	// process commandline and / or wm class (note: this will always return lower case as well)
	char *alt_id = cairo_dock_guess_class (cmdline, wmclass);
	if (!alt_id) return;
	if (!strcmp (alt_id, id_lower))
	{
		g_free (alt_id);
		return;
	}
	
	// only add the alternate ID if it does not exist yet
	if (g_hash_table_contains (db->class_table, alt_id) || g_hash_table_contains(db->alt_class_table, alt_id))
	{
		g_free (alt_id);
		return;
	}
	g_object_ref (desktop_app);
	g_hash_table_insert (db->alt_class_table, alt_id, desktop_app);
}

static gpointer _thread_func (G_GNUC_UNUSED gpointer ptr)
{
	while (1)
	{
		desktop_db *db = NULL;
		GList *list = g_app_info_get_all ();
		
		if (list)
		{
			db = g_malloc (sizeof(desktop_db));
			db->class_table = g_hash_table_new_full (g_str_hash, g_str_equal,
				g_free, g_object_unref);
			db->alt_class_table = g_hash_table_new_full (g_str_hash, g_str_equal,
				g_free, g_object_unref);
			g_list_foreach (list, _process_app, db);
			// note: apps added to the hash table were ref'd in _process_app ()
			g_list_free_full (list, g_object_unref);
		}
		
		gboolean exit = TRUE;
		g_mutex_lock (&mutex);
		if (db)
		{
			_desktop_db_free (db_pending);
			db_pending = db;
			if (more_work) exit = FALSE;
		}
		else error = TRUE;
		more_work = FALSE;
		if (exit) thread_running = FALSE;
		g_cond_signal (&cond);
		g_mutex_unlock (&mutex);
		if (exit) break;
	}
	
	return NULL;
}

static gboolean _start_thread (G_GNUC_UNUSED gpointer ptr)
{
	if (!update_pending) return FALSE;
	g_mutex_lock (&mutex);
	if (thread_running) more_work = TRUE;
	else if (!error)
	{
		if (thread) g_thread_join (thread);
		thread_running = TRUE;
		thread = g_thread_new ("desktop-file-db", _thread_func, NULL);
	}
	g_mutex_unlock (&mutex);
	update_pending = FALSE;
	return FALSE; // needed to remove timeout
}

static void _on_apps_changed(G_GNUC_UNUSED GAppInfoMonitor* pMonitor, G_GNUC_UNUSED void* dummy) {
	if(update_pending) return;
	update_pending = TRUE;
	g_timeout_add_seconds (5, _start_thread, NULL);
}


void gldi_desktop_file_db_init ()
{
	update_pending = TRUE;
	_start_thread (NULL);
	monitor = g_app_info_monitor_get();
	g_signal_connect (monitor, "changed", G_CALLBACK(_on_apps_changed), NULL);
}

void gldi_desktop_file_db_stop (void)
{
	if (monitor)
	{
		g_object_unref (monitor);
		monitor = NULL;
	}
	update_pending = FALSE;
	
	g_mutex_lock (&mutex);
	more_work = FALSE;
	g_mutex_unlock (&mutex);
	
	if (thread)
	{
		g_thread_join (thread);
		thread = NULL;
	}
	_desktop_db_free (db_current);
	_desktop_db_free (db_pending);
	db_current = NULL;
	db_pending = NULL;
	error = FALSE;
}

GDesktopAppInfo *gldi_desktop_file_db_lookup (const char *class, gboolean bOnlyDesktopID)
{
	if (!db_current || g_atomic_pointer_get (&db_pending))
	{
		// update db_current
		g_mutex_lock (&mutex);
		if (!db_pending)
		{
			// in this case db_current == NULL, we have to wait for the thread (which should be running)
			if (!thread_running)
			{
				g_mutex_unlock (&mutex);
				cd_error ("no worker thread!\n");
				return NULL;
			}
			
			while (!(db_pending || error))
				g_cond_wait (&cond, &mutex);
			
			if (error)
			{
				g_mutex_unlock (&mutex);
				cd_error ("cannot get app database!\n");
				return NULL;
			}
		}
		
		// here db_pending is valid, we have to swap with db_current
		_desktop_db_free (db_current);
		db_current = db_pending;
		db_pending = NULL;
		g_mutex_unlock (&mutex);
	}
	
	void *tmp = g_hash_table_lookup (db_current->class_table, class);
	if (!tmp && !bOnlyDesktopID) tmp = g_hash_table_lookup (db_current->alt_class_table, class);
	
	return (GDesktopAppInfo*)tmp;
}



