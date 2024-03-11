if (enable-systemd-service)
	set (unitdir "${prefix}/lib/systemd/user")
	install (FILES cairo-dock.service DESTINATION ${unitdir})
endif()
