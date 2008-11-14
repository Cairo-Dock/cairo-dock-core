
#ifndef __CAIRO_DOCK_ANIMATIONS__
#define  __CAIRO_DOCK_ANIMATIONS__

#include <glib.h>

#include "cairo-dock-struct.h"


gboolean cairo_dock_move_up (CairoDock *pDock);

gboolean cairo_dock_move_down (CairoDock *pDock);

gboolean cairo_dock_pop_up (CairoDock *pDock);

gboolean cairo_dock_pop_down (CairoDock *pDock);


gfloat cairo_dock_calculate_magnitude (gint iMagnitudeIndex);

gboolean cairo_dock_grow_up (CairoDock *pDock);

gboolean cairo_dock_shrink_down (CairoDock *pDock);


/**
*Arme l'animation d'une icone 
*@param icon l'icone dont on veut preparer l'animation.
*@param iAnimationType le type d'animation voulu, ou -1 pour utiliser l'animtion correspondante au type de l'icone.
*@param iNbRounds le nombre de fois ou l'animation sera jouee, ou -1 pour utiliser la valeur correspondante au type de l'icone.
*/
void cairo_dock_arm_animation (Icon *icon, CairoDockAnimationType iAnimationType, int iNbRounds);
/**
*Arme l'animation d'une icone correspondant a un type donne.
*@param icon l'icone a animer.
*@param iType le type d'icone qui sera considere.
*/
void cairo_dock_arm_animation_by_type (Icon *icon, CairoDockIconType iType);
/**
*Lance l'animation de l'icone. Ne fait rien si l'icone ne sera pas animee.
*@param icon l'icone a animer.
*@param pDock le dock contenant l'icone.
*/
void cairo_dock_start_animation (Icon *icon, CairoDock *pDock);

/**
*Definit s'il est utile de lancer l'animation d'un dock (il est inutile de la lancer s'il est manifestement invisible).
*@param pDock le dock a animer.
*/
#define cairo_dock_animation_will_be_visible(pDock) ((pDock)->bInside || (! (pDock)->bAutoHide && (pDock)->iRefCount == 0) || ! (pDock)->bAtBottom)


#endif
