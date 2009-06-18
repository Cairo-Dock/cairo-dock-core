/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

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

static gboolean _cairo_dock_timer (CairoDockTask *pTask)
{
	cairo_dock_launch_task (pTask);
	return TRUE;
}
static gpointer _cairo_dock_threaded_calculation (CairoDockTask *pTask)
{
	//\_______________________ On obtient nos donnees.
	pTask->read (pTask->pSharedMemory);
	
	//\_______________________ On indique qu'on a fini.
	g_atomic_int_set (&pTask->iThreadIsRunning, 0);
	return NULL;
}
static gboolean _cairo_dock_check_for_redraw (CairoDockTask *pTask)
{
	int iThreadIsRunning = g_atomic_int_get (&pTask->iThreadIsRunning);
	if (! iThreadIsRunning)  // le thread a fini.
	{
		//\_______________________ On met a jour avec ces nouvelles donnees et on lance/arrete le timer.
		cairo_dock_perform_task_update (pTask);
		
		pTask->iSidTimerRedraw = 0;
		return FALSE;
	}
	return TRUE;
}
void cairo_dock_launch_task (CairoDockTask *pTask)
{
	g_return_if_fail (pTask != NULL);
	if (pTask->read == NULL)  // pas de thread, tout est dans la fonction d'update.
	{
		cairo_dock_perform_task_update (pTask);
	}
	else
	{
		if (g_atomic_int_compare_and_exchange (&pTask->iThreadIsRunning, 0, 1))  // il etait egal a 0, on lui met 1 et on lance le thread.
		{
			GError *erreur = NULL;
			GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_threaded_calculation, pTask, FALSE, &erreur);
			if (erreur != NULL)  // on n'a pas pu lancer le thread.
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				g_atomic_int_set (&pTask->iThreadIsRunning, 0);
			}
		}
		
		if (pTask->iSidTimerRedraw == 0)
			pTask->iSidTimerRedraw = g_timeout_add (MAX (150, MIN (0.15 * pTask->iPeriod, 333)), (GSourceFunc) _cairo_dock_check_for_redraw, pTask);
	}
}


static gboolean _cairo_dock_one_shot_timer (CairoDockTask *pTask)
{
	pTask->iSidTimerRedraw = 0;
	cairo_dock_launch_task (pTask);
	return FALSE;
}
void cairo_dock_launch_task_delayed (CairoDockTask *pTask, double fDelay)
{
	pTask->iSidTimerRedraw = g_timeout_add (fDelay, (GSourceFunc) _cairo_dock_one_shot_timer, pTask);
}


CairoDockTask *cairo_dock_new_task (int iPeriod, CairoDockReadTimerFunc read, CairoDockUpdateTimerFunc update, gpointer pSharedMemory)
{
	CairoDockTask *pTask = g_new0 (CairoDockTask, 1);
	pTask->iPeriod = iPeriod;
	pTask->read = read;
	pTask->update = update;
	pTask->pSharedMemory = pSharedMemory;
	return pTask;
}


static void _cairo_dock_pause_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	cairo_dock_cancel_next_iteration (pTask);
	
	if (pTask->iSidTimerRedraw != 0)
	{
		g_source_remove (pTask->iSidTimerRedraw);
		pTask->iSidTimerRedraw = 0;
	}
}

void cairo_dock_stop_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	
	_cairo_dock_pause_task (pTask);
	
	cd_message ("***on attend que le thread termine...(%d)", g_atomic_int_get (&pTask->iThreadIsRunning));
	while (g_atomic_int_get (&pTask->iThreadIsRunning))
		g_usleep (10);
		///gtk_main_iteration ();
	cd_message ("***temine.");
}

void cairo_dock_free_task (CairoDockTask *pTask)
{
	if (pTask == NULL)
		return ;
	cairo_dock_stop_task (pTask);
	
	g_free (pTask);
}

gboolean cairo_dock_task_is_active (CairoDockTask *pTask)
{
	return (pTask != NULL && pTask->iSidTimer != 0);
}

gboolean cairo_dock_task_is_running (CairoDockTask *pTask)
{
	return (pTask != NULL && pTask->iSidTimerRedraw != 0);
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
	if (iNewPeriod == -1)  // valeur inchangee.
		iNewPeriod = pTask->iPeriod;
	cairo_dock_change_task_frequency (pTask, iNewPeriod); // nouvelle frequence eventuelement.
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
