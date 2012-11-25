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
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "cairo-dock-log.h"
#include "cairo-dock-task.h"


#define cairo_dock_schedule_next_iteration(pTask) do {\
	if (pTask->iSidTimer == 0 && pTask->iPeriod)\
		pTask->iSidTimer = g_timeout_add_seconds (pTask->iPeriod, (GSourceFunc) _cairo_dock_timer, pTask); } while (0)

#define cairo_dock_cancel_next_iteration(pTask) do {\
	if (pTask->iSidTimer != 0) {\
		g_source_remove (pTask->iSidTimer);\
		pTask->iSidTimer = 0; } } while (0)

#define cairo_dock_perform_task_update(pTask) do {\
	gboolean bContinue = pTask->update (pTask->pSharedMemory);\
	if (! bContinue) {\
		cairo_dock_cancel_next_iteration (pTask); }\
	else {\
		pTask->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;\
		cairo_dock_schedule_next_iteration (pTask); } } while (0)

#define cairo_dock_set_elapsed_time(pTask) do {\
	pTask->fElapsedTime = g_timer_elapsed (pTask->pClock, NULL);\
	g_timer_start (pTask->pClock); } while (0)

#define _free_task(pTask) do {\
	if (pTask->free_data)\
		pTask->free_data (pTask->pSharedMemory);\
	g_timer_destroy (pTask->pClock);\
	g_free (pTask); } while (0)

static gboolean _cairo_dock_timer (CairoDockTask *pTask)
{
	cairo_dock_launch_task (pTask);
	return TRUE;
}
static gboolean _cairo_dock_check_for_update (CairoDockTask *pTask)
{
	// process the data (we don't need to wait that the thread is over, so do it now, it will let more time for the thread to finish, and therfore often save a 'usleep').
	if (pTask->bNeedsUpdate)  // data are ready to be processed -> perform the update
	{
		if (! pTask->bDiscard)  // of course if the task has been discarded before, don't do anything.
		{
			pTask->bContinue = pTask->update (pTask->pSharedMemory);
		}
		pTask->bNeedsUpdate = FALSE;  // now update is done, we won't do it any more until the next iteration, even is we loop on this function.
	}
	
	// finish the iteration, and possibly schedule the next one (the thread must be finished for this part).
	int iThreadIsRunning = g_atomic_int_get (&pTask->iThreadIsRunning);
	if (! iThreadIsRunning)  // if the thread is over
	{
		if (pTask->bDiscard)  // if the task has been discarded, it's the end of the journey for it.
		{
			//g_print ("free discared task...\n");
			_free_task (pTask);
			//g_print ("done.\n");
			return FALSE;
		}
		
		// schedule the next iteration if necessary.
		//cairo_dock_perform_task_update (pTask);
		if (! pTask->bContinue)
		{
			cairo_dock_cancel_next_iteration (pTask);
		}
		else
		{
			pTask->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
			cairo_dock_schedule_next_iteration (pTask);
		}
		pTask->iSidTimerUpdate = 0;
		return FALSE;  // the update is now finished, quit.
	}
	
	// if the thread is not yet over, come back in 1ms.
	g_usleep (1);  // we don't want to block the main loop until the thread is over; so just sleep 1ms to give it a chance to terminate. so it's a kind of 'sched_yield()' wihout blocking the main loop.
	return TRUE;
}
static gpointer _cairo_dock_threaded_calculation (CairoDockTask *pTask)
{
	//\_______________________ get the data
	cairo_dock_set_elapsed_time (pTask);
	pTask->get_data (pTask->pSharedMemory);
	
	// and signal that data are ready to be processed.
	pTask->bNeedsUpdate = TRUE;  // this is only accessed by the update fonction, which is triggered just after, so no need to protect this variable.
	
	//\_______________________ call the update function from the main loop
	if (pTask->iSidTimerUpdate == 0)
		pTask->iSidTimerUpdate = g_idle_add ((GSourceFunc) _cairo_dock_check_for_update, pTask);  // note that 'iSidTimerUpdate' can actually be set after the 'update' is called. that's why the 'update' have to wait for 'iThreadIsRunning == 0' to finish its job.
	
	// and signal that the thread is done.
	g_atomic_int_set (&pTask->iThreadIsRunning, 0);
	return NULL;
}
void cairo_dock_launch_task (CairoDockTask *pTask)
{
	g_return_if_fail (pTask != NULL);
	if (pTask->get_data == NULL)  // pas de thread, tout est dans la fonction d'update.
	{
		cairo_dock_set_elapsed_time (pTask);
		cairo_dock_perform_task_update (pTask);
	}
	else
	{
		if (pTask->iSidTimerUpdate == 0 && g_atomic_int_compare_and_exchange (&pTask->iThreadIsRunning, 0, 1))  // if the task is not running (neither the thread nor the update), set to 1 and launch it. if one of them is running or pending, we don't launch it. so if the task is periodic, it will skip this iteration.
		{
			GError *erreur = NULL;
			#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32)
			GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_threaded_calculation, pTask, FALSE, &erreur);
			#else
			GThread* pThread = g_thread_try_new ("Cairo-Dock Task", (GThreadFunc) _cairo_dock_threaded_calculation, pTask, &erreur);
			g_thread_unref (pThread);
			#endif
			if (erreur != NULL)  // on n'a pas pu lancer le thread.
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				g_atomic_int_set (&pTask->iThreadIsRunning, 0);
			}
		}
	}
}


static gboolean _cairo_dock_one_shot_timer (CairoDockTask *pTask)
{
	pTask->iSidTimer = 0;
	cairo_dock_launch_task (pTask);
	return FALSE;
}
void cairo_dock_launch_task_delayed (CairoDockTask *pTask, double fDelay)
{
	cairo_dock_cancel_next_iteration (pTask);
	if (fDelay == 0)
		pTask->iSidTimer = g_idle_add ((GSourceFunc) _cairo_dock_one_shot_timer, pTask);
	else
		pTask->iSidTimer = g_timeout_add (fDelay, (GSourceFunc) _cairo_dock_one_shot_timer, pTask);
}


CairoDockTask *cairo_dock_new_task_full (int iPeriod, CairoDockGetDataAsyncFunc get_data, CairoDockUpdateSyncFunc update, GFreeFunc free_data, gpointer pSharedMemory)
{
	CairoDockTask *pTask = g_new0 (CairoDockTask, 1);
	pTask->iPeriod = iPeriod;
	pTask->get_data = get_data;
	pTask->update = update;
	pTask->free_data = free_data;
	pTask->pSharedMemory = pSharedMemory;
	pTask->pClock = g_timer_new ();
	return pTask;
}


static void _cairo_dock_pause_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	
	if (pTask->iSidTimerUpdate != 0)
	{
		g_source_remove (pTask->iSidTimerUpdate);
		pTask->iSidTimerUpdate = 0;
	}
}

void cairo_dock_stop_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	_cairo_dock_pause_task (pTask);
	
	cd_message ("***waiting for thread's end...");
	while (g_atomic_int_get (&pTask->iThreadIsRunning))
		g_usleep (10);
	cd_message ("***ended.");
}


/*static gboolean _free_discarded_task (CairoDockTask *pTask)
{
	//g_print ("%s ()\n", __func__);
	cairo_dock_free_task (pTask);
	return FALSE;
}*/
void cairo_dock_discard_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	// mark the task as 'discarded'
	g_atomic_int_set (&pTask->bDiscard, 1);
	
	// if the task is running, there is nothing to do:
	//   if we're inside the thread, it will trigger the 'update' anyway, which will destroy the task.
	//   if we're in the 'update' user callback, the task will be destroyed in the 2nd stage of the function (the user callback is called in the 1st stage).
	if (! cairo_dock_task_is_running (pTask))  // else, we can free the task immediately.
		_free_task (pTask);
	//pTask->iSidTimerUpdate = g_idle_add ((GSourceFunc) _free_discarded_task, pTask);
}

void cairo_dock_free_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	// mark the task as 'discarded'
	g_atomic_int_set (&pTask->bDiscard, 1);
	
	// wait for the thread to finish
	if (! cairo_dock_task_is_running (pTask))
	{
		_free_task (pTask);
	}
	else
	{
		while (g_atomic_int_get (&pTask->iThreadIsRunning))
			g_usleep (10);
	}
	///cairo_dock_stop_task (pTask);
	///_free_task (pTask);
}

gboolean cairo_dock_task_is_active (CairoDockTask *pTask)
{
	return (pTask != NULL && pTask->iSidTimer != 0);
}

gboolean cairo_dock_task_is_running (CairoDockTask *pTask)
{
	return (pTask != NULL && (pTask->iSidTimerUpdate != 0 || g_atomic_int_get (&pTask->iThreadIsRunning)));  // if either the thread is running, or there is a pending update.
}

static void _cairo_dock_restart_timer_with_frequency (CairoDockTask *pTask, int iNewPeriod)
{
	gboolean bNeedsRestart = (pTask->iSidTimer != 0);
	_cairo_dock_pause_task (pTask);
	
	if (bNeedsRestart && iNewPeriod != 0)
		pTask->iSidTimer = g_timeout_add_seconds (iNewPeriod, (GSourceFunc) _cairo_dock_timer, pTask);
}

void cairo_dock_change_task_frequency (CairoDockTask *pTask, int iNewPeriod)
{
	g_return_if_fail (pTask != NULL);
	pTask->iPeriod = iNewPeriod;
	
	_cairo_dock_restart_timer_with_frequency (pTask, iNewPeriod);
}

void cairo_dock_relaunch_task_immediately (CairoDockTask *pTask, int iNewPeriod)
{
	cairo_dock_stop_task (pTask);  // on stoppe avant car on ne veut pas attendre la prochaine iteration.
	if (iNewPeriod >= 0)  // sinon valeur inchangee.
		cairo_dock_change_task_frequency (pTask, iNewPeriod); // nouvelle frequence.
	cairo_dock_launch_task (pTask);  // mesure immediate.
}

void cairo_dock_downgrade_task_frequency (CairoDockTask *pTask)
{
	if (pTask->iFrequencyState < CAIRO_DOCK_FREQUENCY_SLEEP)
	{
		pTask->iFrequencyState ++;
		int iNewPeriod;
		switch (pTask->iFrequencyState)
		{
			case CAIRO_DOCK_FREQUENCY_LOW :
				iNewPeriod = 2 * pTask->iPeriod;
			break ;
			case CAIRO_DOCK_FREQUENCY_VERY_LOW :
				iNewPeriod = 4 * pTask->iPeriod;
			break ;
			case CAIRO_DOCK_FREQUENCY_SLEEP :
				iNewPeriod = 10 * pTask->iPeriod;
			break ;
			default :  // ne doit pas arriver.
				iNewPeriod = pTask->iPeriod;
			break ;
		}
		
		cd_message ("degradation de la mesure (etat <- %d/%d)", pTask->iFrequencyState, CAIRO_DOCK_NB_FREQUENCIES-1);
		_cairo_dock_restart_timer_with_frequency (pTask, iNewPeriod);
	}
}

void cairo_dock_set_normal_task_frequency (CairoDockTask *pTask)
{
	if (pTask->iFrequencyState != CAIRO_DOCK_FREQUENCY_NORMAL)
	{
		pTask->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
		_cairo_dock_restart_timer_with_frequency (pTask, pTask->iPeriod);
	}
}
