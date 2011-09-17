
SET(impl_SRCS
	cairo-dock-gauge.c 				cairo-dock-gauge.h
	cairo-dock-graph.c 				cairo-dock-graph.h
	cairo-dock-hiding-effect.c	 	cairo-dock-hiding-effect.h
	cairo-dock-icon-container.c 	cairo-dock-icon-container.h
	cairo-dock-default-view.c 		cairo-dock-default-view.h
	cairo-dock-compiz-integration.c	cairo-dock-compiz-integration.h
	cairo-dock-kwin-integration.c	cairo-dock-kwin-integration.h
)

add_library(implementations STATIC ${impl_SRCS})
add_definitions (-fPIC)
add_definitions (-DSHARE_DATA_DIR="${pkgdatadir}")

link_directories(
	${PACKAGE_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

########### install files ###############

install (FILES
	cairo-dock-gauge.h	 	 	 cairo-dock-graph.h
	DESTINATION ${includedir}/cairo-dock/implementations)
