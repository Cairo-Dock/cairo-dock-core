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


#ifndef __CAIRO_DIALOG_MANAGER__
#define  __CAIRO_DIALOG_MANAGER__

#include "cairo-dock-container.h"
#include "cairo-dock-dialog-factory.h"
G_BEGIN_DECLS

/** @file cairo-dock-dialog-manager.h This class manages the Dialogs, that are useful to bring interaction with the user.
* 
* With dialogs, you can pop-up messages, ask for question, etc. Any GTK widget can be embedded inside a dialog, giving you any possible interaction with the user.
* 
* Dialogs are constructed with a set of attributes grouped inside a _CairoDialogAttribute. See \ref cairo-dock-dialog-factory.h for the list of available attributes.
* 
* The most generic way to build a Dialog is to fill a _CairoDialogAttribute and pass it to \ref cairo_dock_build_dialog.
* 
* But in most of case, you can just use one of the following convenient functions, that will do the job for you.
* - to show a message, you can use \ref cairo_dock_show_temporary_dialog_with_icon
* - to ask the user a choice, a value or a text, you can use \ref cairo_dock_show_dialog_with_question, \ref cairo_dock_show_dialog_with_value or \ref cairo_dock_show_dialog_with_entry.
* - if you need to block while waiting for the user, use the xxx_and_wait version of these functions.
* - if you want to pop up only 1 dialog at once on a given icon, use \ref cairo_dock_remove_dialog_if_any before you pop up your dialog.
*/


void cairo_dock_init_dialog_manager (void);
void cairo_dock_load_dialog_buttons (CairoContainer *pContainer, gchar *cButtonOkImage, gchar *cButtonCancelImage);
void cairo_dock_unload_dialog_buttons (void);

/** Increase by 1 the reference of a dialog. Use #cairo_dock_dialog_unreference when you're done, so that the dialog can be destroyed.
*@param pDialog the dialog.
*@return TRUE if the reference was not nul, otherwise you must not use it.
*/
gboolean cairo_dock_dialog_reference (CairoDialog *pDialog);

/** Decrease by 1 the reference of a dialog. If the reference becomes nul, the dialog is destroyed.
*@param pDialog the dialog.
*@return TRUE if the reference became nul, in which case the dialog must not be used anymore.
*/
gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog);

/** Unreference the dialogs pointed by an icon.
*@param icon the icon you want to delete all dialogs from.
*@param bAll whether all dialogs should be removed or only the one that don't have interaction with the user.
*@returns TRUE if at least one dialog has been unreferenced.
*/
gboolean cairo_dock_remove_dialog_if_any_full (Icon *icon, gboolean bAll);

/** Unreference all the dialogs pointed by an icon.
*@param icon the icon you want to delete all dialogs from.
*@returns TRUE if at least one dialog has been unreferenced.
*/
#define cairo_dock_remove_dialog_if_any(icon) cairo_dock_remove_dialog_if_any_full (icon, TRUE)



/** Generic function to pop up a dialog.
*@param pAttribute attributes of the dialog.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@return a newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer);


void cairo_dock_replace_all_dialogs (void);

/** Pop up a dialog with a message, a widget, 2 buttons ok/cancel and an icon, all optionnal.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget a GTK widget; It is destroyed with the dialog. Use 'gtk_widget_reparent()' before if you want to keep it alive, or use #cairo_dock_show_dialog_and_wait.
*@param pActionFunc the callback called when the user makes its choice. NULL means there will be no buttons.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data when the dialog is destroyed, or NULL if unnecessary.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a message, and a limited duration, and an icon in the margin.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon.
*@param ... arguments to insert in the message, in a printf way.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_icon_printf (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, ...);

/** Pop up a dialog with a message, and a limited duration, and an icon in the margin.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath);

/** Pop up a dialog with a message, and a limited duration, with no icon.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength);

/** Pop up a dialog with a message, and a limited duration, and a default icon.
*@param cText the format of the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_default_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength);

/** Pop up a dialog with a question and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value "yes" if he clicked on "ok", and with "no" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a text entry and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value of the entry if he clicked on "ok", and with "" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param cTextForEntry text to display initially in the entry.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with an horizontal scale between 0 and fMaxValue and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value of the scale in a text form if he clicked on "ok", and with "-1" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param fValue initial value of the scale.
*@param fMaxValue maximum value of the scale.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, double fValue, double fMaxValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);



/** Pop up a dialog with GTK widget and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength time length of the dialog, or 0 for an unlimited dialog.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget an interactive widget.
*@return GTK_RESPONSE_OK if the user has valideated, GTK_RESPONSE_CANCEL if he cancelled, GTK_RESPONSE_NONE if the dialog has been destroyed before. The dialog is destroyed after the user choosed, but the interactive widget is not destroyed, which allows to retrieve the changes made by the user. Destroy it with 'gtk_widget_destroy' when you're done with it.
*/
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget);

/** Pop up a dialog with a text entry, and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cMessage the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cInitialAnswer the initial value of the entry (can be NULL).
*@return the text entered by the user, or NULL if he cancelled or if the dialog has been destroyed before.
*/
gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer);

/** Pop up a dialog with an horizontal scale between 0 and fMaxValue, and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cMessage the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fInitialValue the initial value of the scale.
*@param fMaxValue the maximum value of the scale.
*@return the value choosed by the user, or -1 if he cancelled or if the dialog has been destroyed before.
*/
double cairo_dock_show_value_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, double fInitialValue, double fMaxValue);

/** Pop up a dialog with a question and 2 buttons yes/no, and block until the user makes its choice.
*@param cQuestion the question to ask.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@return GTK_RESPONSE_YES ou GTK_RESPONSE_NO according to the user's choice, or GTK_RESPONSE_NONE if the dialog has been destroyed before.
*/
int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer);


/** Test if an icon has at least one dialog.
*@param pIcon the icon.
*@return TRUE if the icon has one or more dialog(s).
*/
gboolean cairo_dock_icon_has_dialog (Icon *pIcon);

/** Search the "the best icon possible" to hold a general dialog.
*@return an icon, never NULL except if the dock is.
*/
Icon *cairo_dock_get_dialogless_icon (void);


/** Pop up a dialog, pointing on "the best icon possible". This allows to display a general message.
*@param cMessage the message.
*@param fTimeLength life time of the dialog, in ms.
*@return the newly created dialog, visible and with a reference of 1.
*/
CairoDialog * cairo_dock_show_general_message (const gchar *cMessage, double fTimeLength);

/** Pop up a dialog, pointing on "the best icon possible", and wait. This allows to display a general message.
*@param cQuestion the message.
*@return GTK_RESPONSE_YES ou GTK_RESPONSE_NO according to the user's choice, or GTK_RESPONSE_NONE if the dialog has been destroyed before.
*/
int cairo_dock_ask_general_question_and_wait (const gchar *cQuestion);


/** Hide a dialog.
*@param pDialog the dialog.
*/
void cairo_dock_hide_dialog (CairoDialog *pDialog);
/** Show a dialog and give it focus.
*@param pDialog the dialog.
*/
void cairo_dock_unhide_dialog (CairoDialog *pDialog);
/** Toggle the visibility of a dialog.
*@param pDialog the dialog.
*/
void cairo_dock_toggle_dialog_visibility (CairoDialog *pDialog);


G_END_DECLS
#endif
