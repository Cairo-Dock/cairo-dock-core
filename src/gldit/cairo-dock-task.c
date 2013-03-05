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

/* create task
 * launch -> try-lock mutex or skip, run = 1, new thread or cond_signal
 * thread -> calc -> idle-update -> periodic ? cond_wait (mutex-released) ---> run ? run again : unlock mutex, exit
 *                                           : unlock, unref self, exit
 * update-idle -> update -> while try-lock -> unlock, periodic ? timer to launch again : unref thread, thread = NULL
 */

#ifndef GLIB_VERSION_2_32
#define G_MUTEX_INIT(a)  a = g_mutex_new ()
#define G_COND_INIT(a)   a = g_cond_new ()
#define G_MUTEX_CLEAR(a) g_mutex_free (a)
#define G_COND_CLEAR(a)  g_cond_free (a)
#define G_THREAD_UNREF(t) g_free (t)
#else
#define G_MUTEX_INIT(a)  a = g_new (GMutex, 1); g_mutex_init (a)
#define G_COND_INIT(a)   a = g_new (GCond, 1);  g_cond_init (a)
#define G_MUTEX_CLEAR(a) g_mutex_clear (a); g_free (a)
#define G_COND_CLEAR(a)  g_cond_clear (a);  g_free (a)
#define G_THREAD_UNREF(t) if (t) g_thread_unref (t)
#endif

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
	G_MUTEX_CLEAR (pTask->pMutex);\
	if (pTask->pCond) {\
		G_COND_CLEAR (pTask->pCond); }\
	G_THREAD_UNREF (pTask->pThread);\
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
	if (g_mutex_trylock (pTask->pMutex))  // if the thread is over
	{
		if (pTask->bDiscard)  // if the task has been discarded, it's the end of the journey for it.
		{
			if (pTask->pCond)
			{
				pTask->bRunThread = TRUE;
				g_cond_signal (pTask->pCond);
				g_mutex_unlock (pTask->pMutex);
				g_thread_join (pTask->pThread); // unref the thread
			}
			else
			{
				g_mutex_unlock (pTask->pMutex);
				G_THREAD_UNREF (pTask->pThread);
			}
			pTask->pThread = NULL;
			_free_task (pTask);
			return FALSE;
		}
		
		if (! pTask->pCond)  // one-shot thread => the thread is over
		{
			G_THREAD_UNREF (pTask->pThread);
			pTask->pThread = NULL;
		}
		
		g_mutex_unlock (pTask->pMutex);
		
		// schedule the next iteration if necessary.
		if (! pTask->bContinue)
		{
			cairo_dock_cancel_next_iteration (pTask);
		}
		else
		{
			pTask->iFrequencyState = CAIRO_DOCK_FREQUENCY_NORMAL;
			cairo_dock_schedule_next_iteration (pTask);
		}
		pTask->bIsRunning = FALSE;
		pTask->iSidTimerUpdate = 0;
		return FALSE;  // the update is now finished, quit.
	}
	
	// if the thread is not yet over, come back in 1ms.
	g_usleep (1);  // we don't want to block the main loop until the thread is over; so just sleep 1ms to give it a chance to terminate. so it's a kind of 'sched_yield()' wihout blocking the main loop.
	return TRUE;
}
static gpointer _cairo_dock_threaded_calculation (CairoDockTask *pTask)
{
	g_mutex_lock (pTask->pMutex);
	_run_thread:  // at this point the mutex is locked, either by the first execution of this function, or by 'g_cond_wait'
	
	//\_______________________ get the data
	cairo_dock_set_elapsed_time (pTask);
	pTask->get_data (pTask->pSharedMemory);
	
	// and signal that data are ready to be processed.
	pTask->bNeedsUpdate = TRUE;  // this is only accessed by the update fonction, which is triggered just after, so no need to protect this variable.
	
	//\_______________________ call the update function from the main loop
	if (pTask->iSidTimerUpdate == 0)
		pTask->iSidTimerUpdate = g_idle_add ((GSourceFunc) _cairo_dock_check_for_update, pTask);  // note that 'iSidTimerUpdate' can actually be set after the 'update' is called. that's why the 'update' have to wait for 'iThreadIsRunning == 0' to finish its job.
	
	// sleep until the next iteration or just leave.
	if (pTask->pCond)  // periodic task -> block until the condition becomes TRUE again.
	{
		pTask->bRunThread = FALSE;
		while (! pTask->bRunThread)
			g_cond_wait (pTask->pCond, pTask->pMutex);  // releases the mutex, then takes it again when awakening.
		if (g_atomic_int_get (&pTask->bDiscard) == 0)
			goto _run_thread;
	}
	g_mutex_unlock (pTask->pMutex);
	g_thread_exit (NULL);
	return NULL;
}
void cairo_dock_launch_task (CairoDockTask *pTask)
{
	g_return_if_fail (pTask != NULL);
	if (pTask->get_data == NULL)  // no asynchronous work
	{
		cairo_dock_set_elapsed_time (pTask);
		cairo_dock_perform_task_update (pTask);
	}
	else  // launch the asynchronous work in a thread
	{
		if (pTask->pThread == NULL)  // no thread yet -> create and launch it
		{
			pTask->bIsRunning = TRUE;
			GError *erreur = NULL;
			#ifndef GLIB_VERSION_2_32
			pTask->pThread = g_thread_create ((GThreadFunc) _cairo_dock_threaded_calculation, pTask, TRUE, &erreur);  // TRUE <=> joinable
			#else
			pTask->pThread = g_thread_try_new ("Cairo-Dock Task", (GThreadFunc) _cairo_dock_threaded_calculation, pTask, &erreur);
			#endif
			if (erreur != NULL)  // on n'a pas pu lancer le thread.
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				pTask->bIsRunning = FALSE;
			}
		}
		else  // thread already exists; it's either running or sleeping or finished with a pending update
		if (pTask->pCond && g_mutex_trylock (pTask->pMutex))  // it's a periodic thread, and it's not currently running...
		{
			if (pTask->iSidTimerUpdate == 0)  // ...and it doesn't have a pending update -> awake it and run it again.
			{
				pTask->bRunThread = TRUE;
				pTask->bIsRunning = TRUE;
				g_cond_signal (pTask->pCond);
			}
			g_mutex_unlock (pTask->pMutex);
		}  // else it's a one-shot thread or it's currently running or has a pending update -> don't launch it. so if the task is periodic, it will skip this iteration.
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
	G_MUTEX_INIT (pTask->pMutex);
	if (iPeriod != 0)
	{
		G_COND_INIT (pTask->pCond);
	}
	return pTask;
}


void cairo_dock_stop_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	
	if (cairo_dock_task_is_running (pTask))
	{
		if (pTask->pThread)
		{
			g_atomic_int_set (&pTask->bDiscard, 1);  // set the discard flag to help the 'get_data' callback knows that it should stop.
			if (pTask->pCond)  // the thread might be sleeping, awake it.
			{
				if (g_mutex_trylock (pTask->pMutex))
				{
					pTask->bRunThread = TRUE;
					g_cond_signal (pTask->pCond);
					g_mutex_unlock (pTask->pMutex);
				}
			}
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
			g_atomic_int_set (&pTask->bDiscard, 0);
		}
		if (pTask->iSidTimerUpdate != 0)  // do it after the thread has possibly scheduled the 'update'
		{
			g_source_remove (pTask->iSidTimerUpdate);
			pTask->iSidTimerUpdate = 0;
		}
		pTask->bIsRunning = FALSE;  // since we didn't go through the 'update'
	}
	else
	{
		if (pTask->pThread && pTask->pCond && g_mutex_trylock (pTask->pMutex))  // the thread is sleeping, awake it and let it exit.
		{
			g_atomic_int_set (&pTask->bDiscard, 1);
			pTask->bRunThread = TRUE;
			g_cond_signal (pTask->pCond);
			g_mutex_unlock (pTask->pMutex);
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
			g_atomic_int_set (&pTask->bDiscard, 0);
		}
	}
}


void cairo_dock_discard_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	// mark the task as 'discarded'
	g_atomic_int_set (&pTask->bDiscard, 1);
	
	// if the task is running, there is nothing to do:
	//   if we're inside the thread, it will trigger the 'update' anyway, which will destroy the task.
	//   if we're waiting for the 'update', same as above
	//   if we're inside the 'update' user callback, the task will be destroyed in the 2nd stage of the function (the user callback is called in the 1st stage).
	if (! cairo_dock_task_is_running (pTask))  // we can free the task immediately.
	{
		if (pTask->pThread && pTask->pCond && g_mutex_trylock (pTask->pMutex))  // the thread is sleeping, awake it and let it exit before we can free everything
		{
			pTask->bRunThread = TRUE;
			g_cond_signal (pTask->pCond);
			g_mutex_unlock (pTask->pMutex);
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
		}
		_free_task (pTask);
	}
}

void cairo_dock_free_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_stop_task (pTask);
	_free_task (pTask);
}

gboolean cairo_dock_task_is_active (CairoDockTask *pTask)
{
	return (pTask != NULL && pTask->iSidTimer != 0);
}

gboolean cairo_dock_task_is_running (CairoDockTask *pTask)
{
	return (pTask != NULL && pTask->bIsRunning);
}

static void _cairo_dock_restart_timer_with_frequency (CairoDockTask *pTask, int iNewPeriod)
{
	gboolean bNeedsRestart = (pTask->iSidTimer != 0);
	cairo_dock_cancel_next_iteration (pTask);
	
	if (bNeedsRestart && iNewPeriod != 0)
		pTask->iSidTimer = g_timeout_add_seconds (iNewPeriod, (GSourceFunc) _cairo_dock_timer, pTask);
}

void cairo_dock_change_task_frequency (CairoDockTask *pTask, int iNewPeriod)
{
	g_return_if_fail (pTask != NULL && pTask->iPeriod != 0 && iNewPeriod != 0);
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
