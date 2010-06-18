
find_program (MSGFMT_EXECUTABLE msgfmt)

#on cree une nouvelle cible i18n liée à la cible all.
add_custom_target (i18n ALL COMMENT “Building i18n messages.”)

# on liste tous les .po
file (GLOB PO_FILES *.po)

# on parcours cette liste.
foreach (PO_INPUT ${PO_FILES})
	get_filename_component (PO_INPUT_BASE ${PO_INPUT} NAME_WE)  # NAME Without Extension.
	if (NOT "${PO_INPUT_BASE}" STREQUAL "en_GB") # en_GB has been created only for LaunchPad translation tool
		set (MO_OUTPUT ${PO_INPUT_BASE}.gmo)  # le nom du fichier en sortie.
		message (STATUS "  Building ${MO_OUTPUT}...")
		add_custom_command (TARGET i18n COMMAND ${MSGFMT_EXECUTABLE} -o  ${CMAKE_CURRENT_BINARY_DIR}/${MO_OUTPUT} ${PO_INPUT})  # appel a msgfmt.
		install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/${MO_OUTPUT} DESTINATION ${datadir}/locale/${PO_INPUT_BASE}/LC_MESSAGES RENAME ${GETTEXT_PACKAGE}.mo)  # installation du fichier dans le bon repertoire.
	endif()
endforeach ()
