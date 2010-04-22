
SET(impl_SRCS
	cairo-dock-gauge.c 				cairo-dock-gauge.h
	cairo-dock-graph.c 				cairo-dock-graph.h
	cairo-dock-hiding-effect.c	 		cairo-dock-hiding-effect.h
	cairo-dock-icon-container.c 			cairo-dock-icon-container.h
)

add_library(implementations STATIC ${impl_SRCS})

link_directories(
	${PACKAGE_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/icon-factory)

########### install files ###############

install (FILES
	cairo-dock-gauge.h	 	 	 cairo-dock-graph.h
	DESTINATION ${includedir}/cairo-dock/implementations)
