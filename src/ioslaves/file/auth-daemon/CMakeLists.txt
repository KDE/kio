# SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>
# SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL

set(kio_root_helper_SRCS
	daemon.cpp
	filemanagement.cpp
)

qt_add_dbus_adaptor(kio_root_helper_SRCS
    org.kde.kio.filemanagement.xml

    filemanagement.h FileManagement
)

if (BUILD_AUTH_DAEMON_FUZZER)
	qt_add_dbus_interface(kio_root_helper_SRCS
		org.kde.kio.filemanagement.xml authdaemoninterface
	)
	list(APPEND kio_root_helper_SRCS daemon_fuzzer.cpp)
endif()

configure_file(
	org.kde.kio.filemanagement.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kio.filemanagement.service
)
install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kio.filemanagement.service
    DESTINATION ${KDE_INSTALL_DBUSSYSTEMSERVICEDIR}
)

find_package(PolkitQt${QT_MAJOR_VERSION}-1 REQUIRED)
add_executable(kio_root_helper ${kio_root_helper_SRCS})

install(FILES org.kde.kio.filemanagement.conf DESTINATION ${KDE_INSTALL_DBUSDIR}/system.d)
install(FILES org.kde.kio.filemanagement.policy DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)

target_link_libraries(
	kio_root_helper
	Qt${QT_MAJOR_VERSION}::Core
	Qt${QT_MAJOR_VERSION}::DBus
	PolkitQt${QT_MAJOR_VERSION}-1::Core
	PolkitQt${QT_MAJOR_VERSION}-1::Gui
	PolkitQt${QT_MAJOR_VERSION}-1::Agent
)

install(
	TARGETS kio_root_helper
	DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF}
)
