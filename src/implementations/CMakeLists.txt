
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
	cairo-dock-wayfire-integration.c     cairo-dock-wayfire-integration.h
	cairo-dock-X-manager.c               cairo-dock-X-manager.h
	cairo-dock-X-utilities.c             cairo-dock-X-utilities.h
	cairo-dock-glx.c                     cairo-dock-glx.h
	cairo-dock-egl.c                     cairo-dock-egl.h
	cairo-dock-egl-X.c
	cairo-dock-egl-wayland.c
	cairo-dock-wayland-manager.c         cairo-dock-wayland-manager.h
)
#if ("${HAVE_X11}")
#	SET(impl_SRCS ${impl_SRCS}
#		cairo-dock-X-manager.c               cairo-dock-X-manager.h
#		cairo-dock-glx.c                     cairo-dock-glx.h
#	)
#endif()

if(WaylandScanner_FOUND)
	SET(impl_SRCS
		${impl_SRCS}
		cairo-dock-wayland-wm.c              cairo-dock-wayland-wm.h
		cairo-dock-foreign-toplevel.c        cairo-dock-foreign-toplevel.h
		cairo-dock-cosmic-toplevel.c         cairo-dock-cosmic-toplevel.h
		cairo-dock-plasma-window-manager.c   cairo-dock-plasma-window-manager.h
		cairo-dock-wayland-hotspots.c        cairo-dock-wayland-hotspots.h
	)

	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/wayfire-shell-unstable-v2.xml"
		BASENAME "wayfire-shell")
	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/plasma-window-management.xml"
		BASENAME "plasma-window-management")
	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/wlr-foreign-toplevel-management-unstable-v1.xml"
		BASENAME "wlr-foreign-toplevel-management")
	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/cosmic-toplevel-info-unstable-v1.xml"
		BASENAME "cosmic-toplevel-info")
	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/cosmic-toplevel-management-unstable-v1.xml"
		BASENAME "cosmic-toplevel-management")
	ecm_add_wayland_client_protocol(
		impl_SRCS
		PROTOCOL "proto/cosmic-workspace-unstable-v1.xml"
		BASENAME "cosmic-workspace")
endif()

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
	${CMAKE_SOURCE_DIR}/src/implementations
	${CMAKE_CURRENT_BINARY_DIR})

########### install files ###############

install (FILES
	cairo-dock-gauge.h
	cairo-dock-graph.h
	cairo-dock-progressbar.h
	DESTINATION ${includedir}/cairo-dock/implementations)  # thoses are needed to expose the attributes of the data-renderers to the plug-ins.
