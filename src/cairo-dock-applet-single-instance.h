
#ifndef __CAIRO_DOCK_APPLET_SINGLE_INSTANCE__
#define  __CAIRO_DOCK_APPLET_SINGLE_INSTANCE__


#define CD_APPLET_DEFINE_BEGIN(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
Icon *myIcon; \
CairoContainer *myContainer; \
CairoDock *myDock; \
CairoDesklet *myDesklet; \
cairo_t *myDrawContext; \
AppletConfig *myConfigPtr = NULL; \
AppletData *myDataPtr = NULL; \
CairoDockModuleInstance *myApplet = NULL; \
CD_APPLET_DEFINE_ALL_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
pVisitCard->bMultiInstance = FALSE;

#define CD_APPLET_INIT_BEGIN \
CD_APPLET_INIT_ALL_BEGIN(pApplet) \
myApplet = pApplet; \
myIcon = myApplet->pIcon; \
myContainer = myApplet->pContainer; \
myDock = myApplet->pDock; \
myDesklet = myApplet->pDesklet; \
myDrawContext = myApplet->pDrawContext;

#define myConfig (* myConfigPtr)
#define myData (* myDataPtr)


#define CD_APPLET_RELOAD_BEGIN \
	CD_APPLET_RELOAD_ALL_BEGIN \
	myContainer = myApplet->pContainer; \
	myDock = myApplet->pDock; \
	myDesklet = myApplet->pDesklet; \
	if (CAIRO_DOCK_IS_DESKLET (pOldContainer) && myDrawContext != NULL) { \
		cairo_destroy (myDrawContext); } \
	myDrawContext = myApplet->pDrawContext;


#define CD_APPLET_RESET_DATA_END \
	myDock = NULL; \
	myContainer = NULL; \
	myIcon = NULL; \
	memset (myDataPtr, 0, sizeof (AppletData)); \
	myDataPtr = NULL; \
	if (myDesklet) \
		myApplet->pDrawContext = myDrawContext; \
	myDrawContext = NULL; \
	myDesklet = NULL; \
	myApplet = NULL; \
	CD_APPLET_RESET_DATA_ALL_END


#define CD_APPLET_RESET_CONFIG_BEGIN \
	CD_APPLET_RESET_CONFIG_ALL_BEGIN \
	if (myConfigPtr == NULL) \
		return ;

#define CD_APPLET_RESET_CONFIG_END \
	myConfigPtr = NULL; \
	CD_APPLET_RESET_CONFIG_ALL_END

#define CD_APPLET_GET_CONFIG_BEGIN \
	CD_APPLET_GET_CONFIG_ALL_BEGIN\
	if (myConfigPtr == NULL)\
		myConfigPtr = (((gpointer)myApplet)+sizeof(CairoDockModuleInstance));\
	if (myDataPtr == NULL)\
		myDataPtr = (((gpointer)myConfigPtr)+sizeof(AppletConfig));


extern Icon *myIcon;
extern CairoContainer *myContainer;
extern CairoDock *myDock;
extern CairoDesklet *myDesklet;
extern cairo_t *myDrawContext;
extern AppletConfig *myConfigPtr;
extern AppletData *myDataPtr;
extern CairoDockModuleInstance *myApplet;

#endif
