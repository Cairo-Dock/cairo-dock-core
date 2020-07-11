
SET(impl_SRCS
	gdk-move-to-rect-hack.c            	 gdk-move-to-rect-hack.h
	cairo-dock-gauge.c                   cairo-dock-gauge.h
	cairo-dock-graph.c                   cairo-dock-graph.h
	cairo-dock-progressbar.c             cairo-dock-progressbar.h
	cairo-dock-hiding-effect.c           cairo-dock-hiding-effect.h
	cairo-dock-icon-container.c          cairo-dock-icon-container.h
	cairo-dock-default-view.c            cairo-dock-default-view.h
	cairo-dock-compiz-integration.c      cairo-dock-compiz-integration.h
	cairo-dock-kwin-integration.c        cairo-dock-kwin-integration.h
	cairo-dock-gnome-shell-integration.c cairo-dock-gnome-shell-integration.h
	cairo-dock-cinnamon-integration.c    cairo-dock-cinnamon-integration.h
	cairo-dock-X-manager.c               cairo-dock-X-manager.h
	cairo-dock-X-utilities.c             cairo-dock-X-utilities.h
	cairo-dock-glx.c                     cairo-dock-glx.h
	cairo-dock-egl.c                     cairo-dock-egl.h
	cairo-dock-egl-X.c
	cairo-dock-egl-wayland.c
	cairo-dock-wayland-manager.c         cairo-dock-wayland-manager.h
	cairo-dock-foreign-toplevel.c        cairo-dock-foreign-toplevel.h
	wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
	wlr-foreign-toplevel-management-unstable-v1-protocol.c
)
#if ("${HAVE_X11}")
#	SET(impl_SRCS ${impl_SRCS}
#		cairo-dock-X-manager.c               cairo-dock-X-manager.h
#		cairo-dock-glx.c                     cairo-dock-glx.h
#	)
#endif()

add_library(implementations STATIC ${impl_SRCS})
add_definitions (-fPIC)
add_definitions (-DSHARE_DATA_DIR="${pkgdatadir}")

link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${WAYLAND_LIBRARY_DIRS}
	${GTKLAYERSHELL_LIBRARY_DIRS}
	${EGL_LIBRARY_DIRS}
	${GTK_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIRS}
	${GTKLAYERSHELL_INCLUDE_DIRS}
	${EGL_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/implementations)

########### install files ###############

install (FILES
	cairo-dock-gauge.h
	cairo-dock-graph.h
	cairo-dock-progressbar.h
	DESTINATION ${includedir}/cairo-dock/implementations)  # thoses are needed to expose the attributes of the data-renderers to the plug-ins.
