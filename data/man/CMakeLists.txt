############## Man pages ################
find_program(GZIP_TOOL
				NAMES gzip
				PATHS /bin
				usr/bin
				usr/local/bin)
if(NOT GZIP_TOOL)
	message(FATAL_ERROR "Unable to find 'gzip' program")
endif(NOT GZIP_TOOL)
set(cairo_dock_man_src cairo-dock_en.1) # cairo-dock_fr.1 cairo-dock_it.1)

# Compression of the manuals
foreach(man ${cairo_dock_man_src})
	message(STATUS "Building ${man}.gz")
	execute_process(COMMAND ${GZIP_TOOL} -9c ${CMAKE_CURRENT_SOURCE_DIR}/${man}
						OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${man}.gz)
endforeach(man)

# Installation of the manuals
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock_en.1.gz
        DESTINATION "${mandir}/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME cairo-dock.1.gz)
