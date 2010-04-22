INSTALL(FILES cairo-dock_en.1.gz
        DESTINATION "${mandir}/man1"
        PERMISSIONS OWNER_READ GROUP_READ WORLD_READ RENAME cairo-dock.1.gz)

############## Man pages ################ =>  # doesn't work... #
#FIND_PROGRAM(GZIP_TOOL
#             NAMES gzip
#             PATHS /bin
#                   /usr/bin
#                   /usr/local/bin)
#IF(NOT GZIP_TOOL)
#  MESSAGE(FATAL_ERROR "Unable to find 'gzip' program")
#ENDIF(NOT GZIP_TOOL)
#SET(cairo_dock_man_src cairo-dock_en.1) # cairo-dock_fr.1 cairo-dock_it.1)
#SET(cairo_dock_man_gz cairo-dock_en.1.gz)

# Compression of the manuals
#FOREACH(man ${cairo_dock_man_src})
#  ADD_CUSTOM_COMMAND(OUTPUT ${man}.gz
#                     COMMAND ${GZIP_TOOL} -c ${man} > ${man}.gz
#                     DEPENDS ${man}
#                     COMMENT "Building ${man}.gz")
#ENDFOREACH(man)
# Installation of the manuals
