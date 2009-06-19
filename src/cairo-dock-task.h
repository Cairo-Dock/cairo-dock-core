/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_TASK__
#define  __CAIRO_DOCK_TASK__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-task.h An easy way to define periodic and asynchronous tasks.
* Tasks can of course be synchronous and/or not periodic.
*/


typedef enum {
	CAIRO_DOCK_FREQUENCY_NORMAL = 0,
	CAIRO_DOCK_FREQUENCY_LOW,
	CAIRO_DOCK_FREQUENCY_VERY_LOW,
	CAIRO_DOCK_FREQUENCY_SLEEP,
	CAIRO_DOCK_NB_FREQUENCIES
} CairoDockFrequencyState;

typedef void (* CairoDockGetDataAsyncFunc ) (gpointer pSharedMemory);
typedef gboolean (* CairoDockUpdateSyncFunc ) (gpointer pSharedMemory);
typedef struct {
	/// Sid du timer des mesures.
	gint iSidTimer;
	/// Sid du timer de fin de mesure.
	gint iSidTimerUpdate;
	/// Valeur atomique a 1 ssi le thread de mesure est en cours.
	gint iThreadIsRunning;
	/// fonction realisant l'acquisition asynchrone des donnees; stocke les resultats dans la structures des resultats.
	CairoDockGetDataAsyncFunc get_data;
	/// fonction realisant la mise a jour synchrone de l'IHM en fonction des nouveaux resultats. Renvoie TRUE pour continuer, FALSE pour arreter.
	CairoDockUpdateSyncFunc update;
	/// intervalle de temps en secondes, eventuellement nul pour une mesure unitaire.
	gint iPeriod;
	/// etat de la frequence des mesures.
	CairoDockFrequencyState iFrequencyState;
	/// donnees passees en entree de chaque fonction.
	gpointer pSharedMemory;
} CairoDockTask;

/**
*Lance les mesures periodiques, prealablement preparee avec #cairo_dock_new_task. La 1ere iteration est executee immediatement. L'acquisition et la lecture des donnees est faite de maniere asynchrone (dans un thread secondaire), alors que le chargement des mesures se fait dans la boucle principale. La frequence est remise a son etat normal.
*@param pTask la mesure periodique.
*/
void cairo_dock_launch_task (CairoDockTask *pTask);
/**
*Idem que ci-dessus mais après un délai.
*@param pTask la mesure periodique.
*@param fDelay délai en ms.
*/
void cairo_dock_launch_task_delayed (CairoDockTask *pTask, double fDelay);
/**
*Cree une mesure periodique.
*@param iPeriod l'intervalle en s entre 2 mesures, eventuellement nul pour une mesure unitaire.
*@param acquisition fonction realisant l'acquisition des donnees. N'accede jamais a la structure des resultats.
*@param get_data fonction realisant la lecture des donnees precedemment acquises; stocke les resultats dans la memoire partagee.
*@param update fonction realisant la mise a jour de l'interface en fonction des nouveaux resultats, lus dans la memoire partagee.
*@param pSharedMemory structure passee en entree des fonctions get_data et update. Ne doit pas etre accedee en dehors de ces 2 fonctions !
*@return la mesure nouvellement allouee. A liberer avec #cairo_dock_free_task.
*/
CairoDockTask *cairo_dock_new_task (int iPeriod, CairoDockGetDataAsyncFunc get_data, CairoDockUpdateSyncFunc update, gpointer pSharedMemory);
/**
*Stoppe les mesures. Si une mesure est en cours, le thread d'acquisition/lecture se terminera tout seul plus tard, et la mesure sera ignoree. On peut reprendre les mesures par un simple #cairo_dock_launch_task. Ne doit _pas_ etre appelée durant la fonction 'read' ou 'update'; utiliser la sortie de 'update' pour cela.
*@param pTask la mesure periodique.
*/
void cairo_dock_stop_task (CairoDockTask *pTask);
/**
*Stoppe et detruit une mesure periodique, liberant toutes ses ressources allouees.
*@param pTask la mesure periodique.
*/
void cairo_dock_free_task (CairoDockTask *pTask);
/**
*Dis si une mesure est active, c'est a dire si elle est appelee periodiquement.
*@param pTask la mesure periodique.
*@return TRUE ssi la mesure est active.
*/
gboolean cairo_dock_task_is_active (CairoDockTask *pTask);
/**
*Dis si une mesure est en cours, c'est a dire si elle est soit dans le thread, soit en attente d'update.
*@param pTask la mesure periodique.
*@return TRUE ssi la mesure est en cours.
*/
gboolean cairo_dock_task_is_running (CairoDockTask *pTask);
/**
*Change la frequence des mesures. La prochaine mesure aura lien dans 1 iteration si elle etait deja active.
*@param pTask la mesure periodique.
*@param iNewPeriod le nouvel intervalle entre 2 mesures, en s.
*/
void cairo_dock_change_task_frequency (CairoDockTask *pTask, int iNewPeriod);
/**
*Change la frequence des mesures et les relance immediatement. La prochaine mesure est donc tout de suite.
*@param pTask la mesure periodique.
*@param iNewPeriod le nouvel intervalle entre 2 mesures, en s.
*/
void cairo_dock_relaunch_task_immediately (CairoDockTask *pTask, int iNewPeriod);

/**
*Degrade la frequence des mesures. La mesure passe dans un etat moins actif (typiquement utile si la mesure a echouee).
*@param pTask la mesure periodique.
*/
void cairo_dock_downgrade_task_frequency (CairoDockTask *pTask);
/**
*Remet la frequence des mesures a un etat normal. Notez que cela est fait automatiquement au 1er lancement de la mesure.
*@param pTask la mesure periodique.
*/
void cairo_dock_set_normal_task_frequency (CairoDockTask *pTask);

G_END_DECLS
#endif
