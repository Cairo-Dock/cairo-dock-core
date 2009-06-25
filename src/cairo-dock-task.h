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

/// Type of frequency for a periodic task.
typedef enum {
	CAIRO_DOCK_FREQUENCY_NORMAL = 0,
	CAIRO_DOCK_FREQUENCY_LOW,
	CAIRO_DOCK_FREQUENCY_VERY_LOW,
	CAIRO_DOCK_FREQUENCY_SLEEP,
	CAIRO_DOCK_NB_FREQUENCIES
} CairoDockFrequencyState;

/// Definition of the asynchronous job, that does the heavy part.
typedef void (* CairoDockGetDataAsyncFunc ) (gpointer pSharedMemory);
/// Definition of the synchronous job, that update the dock with the results of the previous job.
typedef gboolean (* CairoDockUpdateSyncFunc ) (gpointer pSharedMemory);

/// Definition of a periodic and asynchronous Task.
struct _CairoDockTask {
	/// ID du timer de la tâche.
	gint iSidTimer;
	/// ID du timer de fin de traitement asynchrone.
	gint iSidTimerUpdate;
	/// Valeur atomique a 1 ssi le thread de tâche est en cours.
	gint iThreadIsRunning;
	/// fonction realisant l'acquisition asynchrone des donnees; stocke les resultats dans la structures des resultats.
	CairoDockGetDataAsyncFunc get_data;
	/// fonction realisant la mise a jour synchrone de l'IHM en fonction des nouveaux resultats. Renvoie TRUE pour continuer, FALSE pour arreter.
	CairoDockUpdateSyncFunc update;
	/// intervalle de temps en secondes, eventuellement nul pour une tâche unitaire.
	gint iPeriod;
	/// etat de la frequence de la tâche.
	CairoDockFrequencyState iFrequencyState;
	/// pSharedMemory structure passee en entree des fonctions get_data et update. Ne doit pas etre accedee en dehors de ces 2 fonctions !
	gpointer pSharedMemory;
} ;

/** Lance une tâche periodique, prealablement preparee avec #cairo_dock_new_task. La 1ere iteration est executee immediatement. L'acquisition et la lecture des donnees est faite de maniere asynchrone (dans un thread secondaire), alors que le chargement de la tâche se fait dans la boucle principale. La frequence est remise a son etat normal.
*@param pTask la tâche periodique.
*/
void cairo_dock_launch_task (CairoDockTask *pTask);
/**
*Idem que ci-dessus mais après un délai.
*@param pTask la tâche periodique.
*@param fDelay délai en ms.
*/
void cairo_dock_launch_task_delayed (CairoDockTask *pTask, double fDelay);
/**
*Cree une tâche periodique.
*@param iPeriod l'intervalle en s entre 2 iterations, eventuellement nul pour une tâche a executer une seule fois.
*@param get_data fonction realisant l'acquisition asynchrone des donnees; stocke les resultats dans la structures des resultats.
*@param update realisant la mise a jour synchrone de l'IHM en fonction des nouveaux resultats. Renvoie TRUE pour continuer, FALSE pour arreter.
*@param pSharedMemory structure passee en entree des fonctions get_data et update. Ne doit pas etre accedee en dehors de ces 2 fonctions !
*@return la tâche nouvellement allouee. A liberer avec #cairo_dock_free_task.
*/
CairoDockTask *cairo_dock_new_task (int iPeriod, CairoDockGetDataAsyncFunc get_data, CairoDockUpdateSyncFunc update, gpointer pSharedMemory);
/**
*Stoppe les tâches. Si une tâche est en cours, le thread d'acquisition/lecture se terminera tout seul plus tard, et la tâche sera ignoree. On peut reprendre les tâches par un simple #cairo_dock_launch_task. Ne doit _pas_ etre appelée durant la fonction 'read' ou 'update'; utiliser la sortie de 'update' pour cela.
*@param pTask la tâche periodique.
*/
void cairo_dock_stop_task (CairoDockTask *pTask);
/**
*Stoppe et detruit une tâche periodique, liberant toutes ses ressources allouees.
*@param pTask la tâche periodique.
*/
void cairo_dock_free_task (CairoDockTask *pTask);
/**
*Dis si une tâche est active, c'est a dire si elle est appelee periodiquement.
*@param pTask la tâche periodique.
*@return TRUE ssi la tâche est active.
*/
gboolean cairo_dock_task_is_active (CairoDockTask *pTask);
/**
*Dis si une tâche est en cours, c'est a dire si elle est soit dans le thread, soit en attente d'update.
*@param pTask la tâche periodique.
*@return TRUE ssi la tâche est en cours.
*/
gboolean cairo_dock_task_is_running (CairoDockTask *pTask);
/**
*Change la frequence de la tâche. La prochaine tâche aura lien dans 1 iteration si elle etait deja active.
*@param pTask la tâche periodique.
*@param iNewPeriod le nouvel intervalle entre 2 tâches, en s.
*/
void cairo_dock_change_task_frequency (CairoDockTask *pTask, int iNewPeriod);
/**
*Change la frequence de la tâche et les relance immediatement. La prochaine tâche est donc tout de suite.
*@param pTask la tâche periodique.
*@param iNewPeriod le nouvel intervalle entre 2 tâches, en s.
*/
void cairo_dock_relaunch_task_immediately (CairoDockTask *pTask, int iNewPeriod);

/**
*Degrade la frequence de la tâche. La tâche passe dans un etat moins actif (typiquement utile si la tâche a echouee).
*@param pTask la tâche periodique.
*/
void cairo_dock_downgrade_task_frequency (CairoDockTask *pTask);
/**
*Remet la frequence de la tâche a un etat normal. Notez que cela est fait automatiquement au 1er lancement de la tâche.
*@param pTask la tâche periodique.
*/
void cairo_dock_set_normal_task_frequency (CairoDockTask *pTask);

G_END_DECLS
#endif
