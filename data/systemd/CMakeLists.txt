if (enable-systemd-service)
	configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock.service.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.service)
	install (FILES ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.service DESTINATION ${unitdir})
endif()
