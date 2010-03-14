
find_program (MSGFMT_EXECUTABLE msgfmt)

#on cree une nouvelle cible i18n liée à la cible all.
add_custom_target (i18n ALL COMMENT “Building i18n messages.”)

# on liste tous les .po
file (GLOB PO_FILES *.po)

# on parcours cette liste.
foreach (PO_INPUT ${PO_FILES})
	get_filename_component (PO_INPUT_BASE ${PO_INPUT} NAME_WE)  # NAME Without Extension.
	set (MO_OUTPUT ${PO_INPUT_BASE}.gmo)  # le nom du fichier en sortie.
	add_custom_command (TARGET i18n COMMAND ${MSGFMT_EXECUTABLE} -o ${MO_OUTPUT} ${PO_INPUT})  # appel a msgfmt.
	install (FILES ${MO_OUTPUT} DESTINATION ${datadir}/locale/${PO_INPUT_BASE}/LC_MESSAGES RENAME ${GETTEXT_PACKAGE}.mo)  # installation du fichier dans le bon repertoire.
endforeach ()
