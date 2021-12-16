# SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>
# SPDX-License-Identifier: BSD-2-Clause

add_executable(kio-geo-uri-handler
    geourihandler.cpp
    main.cpp
)
target_include_directories(kio-geo-uri-handler PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..) # for kio_version.h
target_link_libraries(kio-geo-uri-handler PRIVATE Qt${QT_MAJOR_VERSION}::Gui)

install(TARGETS kio-geo-uri-handler ${INSTALL_TARGETS_DEFAULT_ARGS})
install(
    FILES
        openstreetmap-geo-handler.desktop
        wheelmap-geo-handler.desktop
    DESTINATION
        ${KDE_INSTALL_APPDIR}
)
