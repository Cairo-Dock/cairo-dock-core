/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_TASK__
#define  __CAIRO_DOCK_TASK__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-task.h An easy way to define periodic and asynchronous tasks.
* Tasks can of course be synchronous and/or not periodic.
* 
*/

/// Type of frequency for a periodic task. The frequency of the Task is divided by 2, 4, and 10 for each state.
typedef enum {
	CAIRO_DOCK_FREQUENCY_NORMAL = 0,
	CAIRO_DOCK_FREQUENCY_LOW,
	CAIRO_DOCK_FREQUENCY_VERY_LOW,
	CAIRO_DOCK_FREQUENCY_SLEEP,
	CAIRO_DOCK_NB_FREQUENCIES
} CairoDockFrequencyState;

/// Definition of the asynchronous job, that does the heavy part.
typedef void (* CairoDockGetDataAsyncFunc ) (gpointer pSharedMemory);
/// Definition of the synchronous job, that update the dock with the results of the previous job. Returns TRUE to continue, FALSE to stop
typedef gboolean (* CairoDockUpdateSyncFunc ) (gpointer pSharedMemory);

/// Definition of a periodic and asynchronous Task.
struct _CairoDockTask {
	/// ID of the timer of the Task.
	gint iSidTimer;
	/// ID of the timer to check the end of the thread.
	gint iSidTimerUpdate;
	/// Atomic value, set to 1 when the thread is running.
	gint iThreadIsRunning;
	/// function carrying out the heavy job.
	CairoDockGetDataAsyncFunc get_data;
	/// function carrying out the update of the dock. Returns TRUE to continue, FALSE to stop.
	CairoDockUpdateSyncFunc update;
	/// intervalle de temps en secondes, eventuellement nul pour une Task unitaire.
	gint iPeriod;
	/// etat of the frequency of the Task.
	CairoDockFrequencyState iFrequencyState;
	/// pSharedMemory structure passee en entree des fonctions get_data et update. Ne doit pas etre accedee en dehors de ces 2 fonctions !
	gpointer pSharedMemory;
} ;


/** Launch a periodic Task, beforehand prepared with #cairo_dock_new_task. The first iteration is executed immediately. The frequency returns to its normal state.
*@param pTask the periodic Task.
*/
void cairo_dock_launch_task (CairoDockTask *pTask);

/** Same as above but after a delay.
*@param pTask the periodic Task.
*@param fDelay delay in ms.
*/
void cairo_dock_launch_task_delayed (CairoDockTask *pTask, double fDelay);

/** Create a periodic Task.
*@param iPeriod time between 2 iterations, possibly nul for a Task to be executed once only.
*@param get_data asynchonous function, which carries out the heavy job parallel to the dock; stores the results in the shared memory.
*@param update synchonous function, which carries out the update of the dock from the result of the previous function. Returns TRUE to continue, FALSE to stop.
*@param pSharedMemory structure passed as a parameter of the get_data and update functions. Must not be accessed outside of these  functions !
*@return the newly allocated Task, ready to be launched with #cairo_dock_launch_task. Free it with #cairo_dock_free_task.
*/
CairoDockTask *cairo_dock_new_task (int iPeriod, CairoDockGetDataAsyncFunc get_data, CairoDockUpdateSyncFunc update, gpointer pSharedMemory);

/** Stop a periodic Task. If the Task is running, it will wait until the asynchronous thread has finished, and skip the update. The Task can be launched again with a call to #cairo_dock_launch_task.
*@param pTask the periodic Task.
*/
void cairo_dock_stop_task (CairoDockTask *pTask);

/** Stop and destroy a periodic Task, freeing all the allocated ressources.
*@param pTask the periodic Task.
*/
void cairo_dock_free_task (CairoDockTask *pTask);

/** Tell if a Task is active, that is to say is periodically called.
*@param pTask the periodic Task.
*@return TRUE if the Task is active.
*/
gboolean cairo_dock_task_is_active (CairoDockTask *pTask);

/** Tell if a Task is running, that is to say it is either in the thread or waiting for the update.
*@param pTask the periodic Task.
*@return TRUE if the Task is running.
*/
gboolean cairo_dock_task_is_running (CairoDockTask *pTask);

/** Change the frequency of a Task. The next iteration is re-scheduled according to the new period.
*@param pTask the periodic Task.
*@param iNewPeriod the new period between 2 iterations of the Task, in s.
*/
void cairo_dock_change_task_frequency (CairoDockTask *pTask, int iNewPeriod);
/** Change the frequency of a Task and relaunch it immediately. The next iteration is therefore immediately executed.
*@param pTask the periodic Task.
*@param iNewPeriod the new period between 2 iterations of the Task, in s.
*/
void cairo_dock_relaunch_task_immediately (CairoDockTask *pTask, int iNewPeriod);

/** Downgrade the frequency of a Task. The Task will be executed less often (this is typically useful to put on stand-by a periodic measure).
*@param pTask the periodic Task.
*/
void cairo_dock_downgrade_task_frequency (CairoDockTask *pTask);
/** Set the frequency of the Task to its normal state. This is also done automatically when launching the Task.
*@param pTask the periodic Task.
*/
void cairo_dock_set_normal_task_frequency (CairoDockTask *pTask);


G_END_DECLS
#endif
