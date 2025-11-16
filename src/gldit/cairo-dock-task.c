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

#define _schedule_next_iteration(pTask) do {\
	if (pTask->iSidTimer == 0 && pTask->iPeriod)\
		pTask->iSidTimer = g_timeout_add_seconds (pTask->iPeriod, (GSourceFunc) _launch_task_timer, pTask); } while (0)

#define _cancel_next_iteration(pTask) do {\
	if (pTask->iSidTimer != 0) {\
		g_source_remove (pTask->iSidTimer);\
		pTask->iSidTimer = 0; } } while (0)

#define _set_elapsed_time(pTask) do {\
	pTask->fElapsedTime = g_timer_elapsed (pTask->pClock, NULL);\
	g_timer_start (pTask->pClock); } while (0)

static void _free_task (GldiTask *pTask)
{
	if (pTask->free_data)
		pTask->free_data (pTask->pSharedMemory);
	g_timer_destroy (pTask->pClock);
	g_mutex_clear (&pTask->mutex);
	if (pTask->iPeriod) g_cond_clear (&pTask->cond);
	if (pTask->pThread) g_thread_unref (pTask->pThread);
	g_free (pTask);
}

static gboolean _launch_task_timer (GldiTask *pTask)
{
	gldi_task_launch (pTask);
	return TRUE;
}
static gboolean _check_for_update_idle (GldiTask *pTask)
{
	// process the data (we don't need to wait that the thread is over, so do it now, it will let more time for the thread to finish, and therefore often save a 'usleep').
	if (pTask->bNeedsUpdate)  // data are ready to be processed -> perform the update
	{
		if (! pTask->bDiscard)  // of course if the task has been discarded before, don't do anything.
		{
			pTask->bContinue = pTask->update (pTask->pSharedMemory);
		}
		pTask->bNeedsUpdate = FALSE;  // now update is done, we won't do it any more until the next iteration, even is we loop on this function.
	}
	
	// finish the iteration, and possibly schedule the next one (the thread must be finished for this part).
	if (g_mutex_trylock (&pTask->mutex))  // if the thread is over
	{
		if (pTask->bDiscard)  // if the task has been discarded, it's the end of the journey for it.
		{
			if (pTask->iPeriod)
			{
				pTask->bRunThread = TRUE;
				g_cond_signal (&pTask->cond);
				g_mutex_unlock (&pTask->mutex);
				g_thread_join (pTask->pThread); // unref the thread
				pTask->pThread = NULL;
			}
			else g_mutex_unlock (&pTask->mutex);

			_free_task (pTask);
			return FALSE;
		}
		
		if (! pTask->iPeriod)  // one-shot thread => the thread is over
		{
			if (pTask->pThread)
			{
				g_thread_unref (pTask->pThread);
				pTask->pThread = NULL;
			}
		}
		
		pTask->iSidUpdateIdle = 0;  // set it before the unlock, as it is accessed in the thread part
		g_mutex_unlock (&pTask->mutex);
		
		// schedule the next iteration if necessary.
		if (! pTask->bContinue)
		{
			_cancel_next_iteration (pTask);
		}
		else
		{
			pTask->iFrequencyState = GLDI_TASK_FREQUENCY_NORMAL;
			_schedule_next_iteration (pTask);
		}
		pTask->bIsRunning = FALSE;
		return FALSE;  // the update is now finished, quit.
	}
	
	// if the thread is not yet over, come back in 1ms.
	g_usleep (1);  // we don't want to block the main loop until the thread is over; so just sleep 1ms to give it a chance to terminate. so it's a kind of 'sched_yield()' without blocking the main loop.
	return TRUE;
}
static gpointer _get_data_threaded (GldiTask *pTask)
{
	g_mutex_lock (&pTask->mutex);
	_run_thread:  // at this point the mutex is locked, either by the first execution of this function, or by 'g_cond_wait'
	
	//\_______________________ get the data
	_set_elapsed_time (pTask);
	pTask->get_data (pTask->pSharedMemory);
	
	// and signal that data are ready to be processed.
	pTask->bNeedsUpdate = TRUE;  // this is only accessed by the update fonction, which is triggered just after, so no need to protect this variable.
	
	//\_______________________ call the update function from the main loop
	if (pTask->iSidUpdateIdle == 0)
		pTask->iSidUpdateIdle = g_idle_add ((GSourceFunc) _check_for_update_idle, pTask);  // note that 'iSidUpdateIdle' can actually be set after the 'update' is called. that's why the 'update' have to wait for the mutex to finish its job.
	
	// sleep until the next iteration or just leave.
	if (pTask->iPeriod)  // periodic task -> block until the condition becomes TRUE again.
	{
		pTask->bRunThread = FALSE;
		while (! pTask->bRunThread)
			g_cond_wait (&pTask->cond, &pTask->mutex);  // releases the mutex, then takes it again when awakening.
		if (g_atomic_int_get (&pTask->bDiscard) == 0)
			goto _run_thread;
	}
	g_mutex_unlock (&pTask->mutex);
	g_thread_exit (NULL);
	return NULL;
}
void gldi_task_launch (GldiTask *pTask)
{
	g_return_if_fail (pTask != NULL);
	if (pTask->get_data == NULL)  // no asynchronous work -> just call the 'update' and directly schedule the next iteration
	{
		_set_elapsed_time (pTask);
		pTask->bContinue = pTask->update (pTask->pSharedMemory);
		if (! pTask->bContinue)
		{
			_cancel_next_iteration (pTask);
		}
		else
		{
			pTask->iFrequencyState = GLDI_TASK_FREQUENCY_NORMAL;
			_schedule_next_iteration (pTask);
		}
	}
	else  // launch the asynchronous work in a thread
	{
		if (pTask->pThread == NULL)  // no thread yet -> create and launch it
		{
			pTask->bIsRunning = TRUE;
			GError *erreur = NULL;
			pTask->pThread = g_thread_try_new ("Cairo-Dock Task", (GThreadFunc) _get_data_threaded, pTask, &erreur);
			if (erreur != NULL)  // on n'a pas pu lancer le thread.
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				pTask->bIsRunning = FALSE;
			}
		}
		else  // thread already exists; it's either running or sleeping or finished with a pending update
		if (pTask->iPeriod && g_mutex_trylock (&pTask->mutex))  // it's a periodic thread, and it's not currently running...
		{
			if (pTask->iSidUpdateIdle == 0)  // ...and it doesn't have a pending update -> awake it and run it again.
			{
				pTask->bRunThread = TRUE;
				pTask->bIsRunning = TRUE;
				g_cond_signal (&pTask->cond);
			}
			g_mutex_unlock (&pTask->mutex);
		}  // else it's a one-shot thread or it's currently running or has a pending update -> don't launch it. so if the task is periodic, it will skip this iteration.
	}
}


static gboolean _one_shot_timer (GldiTask *pTask)
{
	pTask->iSidTimer = 0;
	gldi_task_launch (pTask);
	return FALSE;
}
void gldi_task_launch_delayed (GldiTask *pTask, guint delay)
{
	_cancel_next_iteration (pTask);
	if (delay == 0)
		pTask->iSidTimer = g_idle_add ((GSourceFunc) _one_shot_timer, pTask);
	else
		pTask->iSidTimer = g_timeout_add (delay, (GSourceFunc) _one_shot_timer, pTask);
}


GldiTask *gldi_task_new_full (int iPeriod, GldiGetDataAsyncFunc get_data, GldiUpdateSyncFunc update, GFreeFunc free_data, gpointer pSharedMemory)
{
	GldiTask *pTask = g_new0 (GldiTask, 1);
	pTask->iPeriod = iPeriod;
	pTask->get_data = get_data;
	pTask->update = update;
	pTask->free_data = free_data;
	pTask->pSharedMemory = pSharedMemory;
	pTask->pClock = g_timer_new ();
	g_mutex_init (&pTask->mutex);
	if (iPeriod != 0)
	{
		g_cond_init (&pTask->cond);
	}
	return pTask;
}


void gldi_task_stop (GldiTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	_cancel_next_iteration (pTask);
	
	if (gldi_task_is_running (pTask))
	{
		if (pTask->pThread)
		{
			g_atomic_int_set (&pTask->bDiscard, 1);  // set the discard flag to help the 'get_data' callback knows that it should stop.
			if (pTask->iPeriod)  // the thread might be sleeping, awake it.
			{
				if (g_mutex_trylock (&pTask->mutex))
				{
					pTask->bRunThread = TRUE;
					g_cond_signal (&pTask->cond);
					g_mutex_unlock (&pTask->mutex);
				}
			}
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
			g_atomic_int_set (&pTask->bDiscard, 0);
		}
		if (pTask->iSidUpdateIdle != 0)  // do it after the thread has possibly scheduled the 'update'
		{
			g_source_remove (pTask->iSidUpdateIdle);
			pTask->iSidUpdateIdle = 0;
		}
		pTask->bIsRunning = FALSE;  // since we didn't go through the 'update'
	}
	else
	{
		if (pTask->pThread && pTask->iPeriod && g_mutex_trylock (&pTask->mutex))  // the thread is sleeping, awake it and let it exit.
		{
			g_atomic_int_set (&pTask->bDiscard, 1);
			pTask->bRunThread = TRUE;
			g_cond_signal (&pTask->cond);
			g_mutex_unlock (&pTask->mutex);
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
			g_atomic_int_set (&pTask->bDiscard, 0);
		}
	}
}


void gldi_task_discard (GldiTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	_cancel_next_iteration (pTask);
	// mark the task as 'discarded'
	g_atomic_int_set (&pTask->bDiscard, 1);
	
	// if the task is running, there is nothing to do:
	//   if we're inside the thread, it will trigger the 'update' anyway, which will destroy the task.
	//   if we're waiting for the 'update', same as above
	//   if we're inside the 'update' user callback, the task will be destroyed in the 2nd stage of the function (the user callback is called in the 1st stage).
	if (! gldi_task_is_running (pTask))  // we can free the task immediately.
	{
		if (pTask->pThread && pTask->iPeriod && g_mutex_trylock (&pTask->mutex))  // the thread is sleeping, awake it and let it exit before we can free everything
		{
			pTask->bRunThread = TRUE;
			g_cond_signal (&pTask->cond);
			g_mutex_unlock (&pTask->mutex);
			g_thread_join (pTask->pThread);  // unref the thread
			pTask->pThread = NULL;
		}
		_free_task (pTask);
	}
}

void gldi_task_free (GldiTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	gldi_task_stop (pTask);
	_free_task (pTask);
}

gboolean gldi_task_is_active (GldiTask *pTask)
{
	return (pTask != NULL && pTask->iSidTimer != 0);
}

gboolean gldi_task_is_running (GldiTask *pTask)
{
	return (pTask != NULL && pTask->bIsRunning);
}

static void _restart_timer_with_frequency (GldiTask *pTask, int iNewPeriod)
{
	gboolean bNeedsRestart = (pTask->iSidTimer != 0);
	_cancel_next_iteration (pTask);
	
	if (bNeedsRestart && iNewPeriod != 0)
		pTask->iSidTimer = g_timeout_add_seconds (iNewPeriod, (GSourceFunc) _launch_task_timer, pTask);
}

void gldi_task_change_frequency (GldiTask *pTask, int iNewPeriod)
{
	g_return_if_fail (pTask != NULL && pTask->iPeriod != 0 && iNewPeriod != 0);
	pTask->iPeriod = iNewPeriod;
	
	_restart_timer_with_frequency (pTask, iNewPeriod);
}

void gldi_task_change_frequency_and_relaunch (GldiTask *pTask, int iNewPeriod)
{
	gldi_task_stop (pTask);  // on stoppe avant car on ne veut pas attendre la prochaine iteration.
	if (iNewPeriod >= 0)  // sinon valeur inchangee.
		gldi_task_change_frequency (pTask, iNewPeriod); // nouvelle frequence.
	gldi_task_launch (pTask);  // mesure immediate.
}

void gldi_task_downgrade_frequency (GldiTask *pTask)
{
	if (pTask->iFrequencyState < GLDI_TASK_FREQUENCY_SLEEP)
	{
		pTask->iFrequencyState ++;
		int iNewPeriod;
		switch (pTask->iFrequencyState)
		{
			case GLDI_TASK_FREQUENCY_LOW :
				iNewPeriod = 2 * pTask->iPeriod;
			break ;
			case GLDI_TASK_FREQUENCY_VERY_LOW :
				iNewPeriod = 4 * pTask->iPeriod;
			break ;
			case GLDI_TASK_FREQUENCY_SLEEP :
				iNewPeriod = 10 * pTask->iPeriod;
			break ;
			default :  // ne doit pas arriver.
				iNewPeriod = pTask->iPeriod;
			break ;
		}
		
		cd_message ("degradation de la mesure (etat <- %d/%d)", pTask->iFrequencyState, GLDI_TASK_NB_FREQUENCIES-1);
		_restart_timer_with_frequency (pTask, iNewPeriod);
	}
}

void gldi_task_set_normal_frequency (GldiTask *pTask)
{
	if (pTask->iFrequencyState != GLDI_TASK_FREQUENCY_NORMAL)
	{
		pTask->iFrequencyState = GLDI_TASK_FREQUENCY_NORMAL;
		_restart_timer_with_frequency (pTask, pTask->iPeriod);
	}
}
