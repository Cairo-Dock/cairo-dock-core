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

#ifndef __CAIRO_DOCK_TASK__
#define  __CAIRO_DOCK_TASK__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-task.h An easy way to define periodic and asynchronous tasks, that can perform heavy jobs without blocking the dock.
 *
 *  A Task is divided in 2 phases : 
 * - the asynchronous phase will be executed in another thread, while the dock continues to run on its own thread, in parallel. During this phase you will do all the heavy job (like downloading a file or computing something) but you can't interact on the dock.
 * - the synchronous phase will be executed after the first one has finished. There you will update your applet with the result of the first phase.
 * 
 * \attention A data buffer is used to communicate between the 2 phases. It is important that these datas are never accessed outside the task, and vice versa that the asynchronous thread never accesses other data than this buffer.\n
 * If you want to access these datas outside the task, you have to copy them in a safe place during the 2nd phase, or to stop the task before (beware that stopping the task means waiting for the 1st phase to finish, which can take some time).
 * 
 * You create a Task with \ref gldi_task_new, launch it with \ref gldi_task_launch, and destroy it with \ref gldi_task_free or \ref gldi_task_discard.
 *
 * A Task can be periodic if you specify a period, otherwise it will be executed once. It also can also be fully synchronous if you don't specify an asynchronous function.
 * 
 */

// Type of frequency for a periodic task. The frequency of the Task is divided by 2, 4, and 10 for each state.
typedef enum {
	GLDI_TASK_FREQUENCY_NORMAL = 0,
	GLDI_TASK_FREQUENCY_LOW,
	GLDI_TASK_FREQUENCY_VERY_LOW,
	GLDI_TASK_FREQUENCY_SLEEP,
	GLDI_TASK_NB_FREQUENCIES
} GldiTaskFrequencyState;

/// Definition of the asynchronous job, that does the heavy part.
typedef void (* GldiGetDataAsyncFunc ) (gpointer pSharedMemory);
/// Definition of the synchronous job, that update the dock with the results of the previous job. Returns TRUE to continue, FALSE to stop
typedef gboolean (* GldiUpdateSyncFunc ) (gpointer pSharedMemory);

/// Definition of a periodic and/or asynchronous Task.
struct _GldiTask {
	// ID of the timer of the Task (if periodic)
	gint iSidTimer;
	// TRUE if the thread is running or about to run or if the update is pending
	gboolean bIsRunning;
	// function carrying out the heavy job.
	GldiGetDataAsyncFunc get_data;
	// function carrying out the update of the dock. Returns TRUE to continue, FALSE to stop.
	GldiUpdateSyncFunc update;
	/// interval of time in seconds, 0 if the Task is to run once.
	guint iPeriod;
	// state of the frequency of the Task.
	GldiTaskFrequencyState iFrequencyState;
	// timer to get the accurate amount of time since last update.
	GTimer *pClock;
	// time elapsed since last update.
	double fElapsedTime;
	// function called when the task is destroyed to free the shared memory (optionnal).
	GFreeFunc free_data;
	// below are the parameters accessed inside the thread => only between mutex lock/unlock
	/// structure passed as parameter of the 'get_data' and 'update' functions. Must not be accessed outside of these 2 functions !
	gpointer pSharedMemory;
	// ID of the idle source to perform the update.
	gint iSidUpdateIdle;
	/// TRUE when the task has been discarded.
	gboolean bDiscard;
	gboolean bNeedsUpdate;  // TRUE when new data are waiting to be processed.
	gboolean bContinue;  // result of the 'update' function (TRUE -> continue, FALSE -> stop, if the task is periodic).
	GThread *pThread;  // the thread that execute the asynchronous 'get_data' callback
	GCond *pCond;  // condition to awake the thread (if periodic).
	gboolean bRunThread;  // condition value: whether to run the thread or exit.
	GMutex *pMutex;  // mutex associated with the condition.
} ;


/** Launch a periodic Task, beforehand prepared with #gldi_task_new. The first iteration is executed immediately. The frequency returns to its normal state.
*@param pTask the periodic Task.
*/
void gldi_task_launch (GldiTask *pTask);

/** Same as above but after a delay. If the delay is 0, the task will be launched as soon as the main loop becomes idle.
*@param pTask the periodic Task.
*@param fDelay delay in ms.
*/
void gldi_task_launch_delayed (GldiTask *pTask, guint delay);

/** Create a periodic Task.
*@param iPeriod time between 2 iterations, possibly nul for a Task to be executed once only.
*@param get_data asynchonous function, which carries out the heavy job parallel to the dock; stores the results in the shared memory.
*@param update synchonous function, which carries out the update of the dock from the result of the previous function. Returns TRUE to continue, FALSE to stop.
*@param free_data function called when the Task is destroyed, to free the shared memory (optionnal).
*@param pSharedMemory structure passed as a parameter of the get_data and update functions. Must not be accessed outside of these functions !
*@return the newly allocated Task, ready to be launched with \ref gldi_task_launch. Free it with \ref gldi_task_free or \ref gldi_task_discard.
*/
GldiTask *gldi_task_new_full (int iPeriod, GldiGetDataAsyncFunc get_data, GldiUpdateSyncFunc update, GFreeFunc free_data, gpointer pSharedMemory);

/** Create a periodic Task.
*@param iPeriod time between 2 iterations, possibly nul for a Task to be executed once only.
*@param get_data asynchonous function, which carries out the heavy job parallel to the dock; stores the results in the shared memory.
*@param update synchonous function, which carries out the update of the dock from the result of the previous function. Returns TRUE to continue, FALSE to stop.
*@param pSharedMemory structure passed as a parameter of the get_data and update functions. Must not be accessed outside of these  functions !
*@return the newly allocated Task, ready to be launched with \ref gldi_task_launch. Free it with \ref gldi_task_free or \ref gldi_task_discard.
*/
#define gldi_task_new(iPeriod, get_data, update, pSharedMemory) gldi_task_new_full (iPeriod, get_data, update, NULL, pSharedMemory)

/** Stop a periodic Task. If the Task is running, it will wait until the asynchronous thread has finished, and skip the update. The Task can be launched again with a call to #gldi_task_launch.
*@param pTask the periodic Task.
*/
void gldi_task_stop (GldiTask *pTask);

/** Discard a periodic Task. The asynchronous thread will continue, and the Task will be freed when it ends. The Task should be considered as destroyed after a call to this function. This function can be used inside the 'update' callback to destroy the Task.
*@param pTask the periodic Task.
*/
void gldi_task_discard (GldiTask *pTask);

/** Stop and destroy a periodic Task, freeing all the allocated ressources. Unlike \ref gldi_task_discard, the task is stopped before being freeed, so this is a blocking call. If you want to destroy the task inside the update callback, don't use this function; use \ref gldi_task_discard instead.
*@param pTask the periodic Task.
*/
void gldi_task_free (GldiTask *pTask);

/** Tell if a Task is active, that is to say is periodically called.
*@param pTask the periodic Task.
*@return TRUE if the Task is active.
*/
gboolean gldi_task_is_active (GldiTask *pTask);

/** Tell if a Task is running, that is to say it is either in the thread or waiting for the update.
*@param pTask the periodic Task.
*@return TRUE if the Task is running.
*/
gboolean gldi_task_is_running (GldiTask *pTask);

/** Change the frequency of a Task. The next iteration is re-scheduled according to the new period.
*@param pTask the periodic Task.
*@param iNewPeriod the new period between 2 iterations of the Task, in s.
*/
void gldi_task_change_frequency (GldiTask *pTask, int iNewPeriod);
/** Change the frequency of a Task and relaunch it immediately. The next iteration is therefore immediately executed.
*@param pTask the periodic Task.
*@param iNewPeriod the new period between 2 iterations of the Task, in s, or -1 to let it unchanged.
*/
void gldi_task_change_frequency_and_relaunch (GldiTask *pTask, int iNewPeriod);

/** Downgrade the frequency of a Task. The Task will be executed less often (this is typically useful to put on stand-by a periodic measure).
*@param pTask the periodic Task.
*/
void gldi_task_downgrade_frequency (GldiTask *pTask);
/** Set the frequency of the Task to its normal state. This is also done automatically when launching the Task.
*@param pTask the periodic Task.
*/
void gldi_task_set_normal_frequency (GldiTask *pTask);

/** Get the time elapsed since the last time the Task has run.
*@param pTask the periodic Task.
*/
#define gldi_task_get_elapsed_time(pTask) (pTask->fElapsedTime)

G_END_DECLS
#endif
