configure_file(config-kiogui.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiogui.h)

add_library(KF5KIOGui)
add_library(KF5::KIOGui ALIAS KF5KIOGui)

set_target_properties(KF5KIOGui PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOGui
)

target_sources(KF5KIOGui PRIVATE
   applicationlauncherjob.cpp
   commandlauncherjob.cpp
   faviconrequestjob.cpp
   kcoreurlnavigator.cpp
   openurljob.cpp
   openwithhandlerinterface.cpp
   openorexecutefileinterface.cpp
   kprocessrunner.cpp
   kterminallauncherjob.cpp
   kemailclientlauncherjob.cpp
   dbusactivationrunner.cpp
   thumbnailcreator.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEMD_DBUS_XMLS
        systemd/org.freedesktop.systemd1.Manager.xml
        systemd/org.freedesktop.systemd1.Unit.xml
        systemd/org.freedesktop.DBus.Properties.xml)

    set_source_files_properties(${SYSTEMD_DBUS_XMLS} PROPERTIES INCLUDE systemd/dbustypes.h)

    set(kiogui_dbus_SRCS)
    qt_add_dbus_interfaces(kiogui_dbus_SRCS ${SYSTEMD_DBUS_XMLS})

    target_sources(KF5KIOGui PRIVATE
        ${kiogui_dbus_SRCS}
        systemd/systemdprocessrunner.cpp
        systemd/scopedprocessrunner.cpp
    )
endif ()

ecm_qt_declare_logging_category(KF5KIOGui
    HEADER kiogui_debug.h
    IDENTIFIER KIO_GUI
    CATEGORY_NAME kf.kio.gui
    OLD_CATEGORY_NAMES kf5.kio.gui
    DESCRIPTION "KIOGui (KIO)"
    EXPORT KIO
)

ecm_qt_declare_logging_category(KF5KIOGui
    HEADER favicons_debug.h
    IDENTIFIER FAVICONS_LOG
    CATEGORY_NAME kf.kio.gui.favicons
    OLD_CATEGORY_NAMES kf5.kio.favicons
    DESCRIPTION "FavIcons (KIO)"
    EXPORT KIO
)

ecm_generate_export_header(KF5KIOGui
    BASE_NAME KIOGui
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.101 5.103
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF5KIOGui INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui>")

target_link_libraries(KF5KIOGui
  PUBLIC
    KF5::KIOCore
    KF5::ConfigCore
    KF5::Service
    Qt${QT_MAJOR_VERSION}::Gui
  PRIVATE
    KF5::I18n
)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.102.0)
  target_link_libraries(KF5KIOGui PUBLIC KF5::WindowSystem)
else()
  target_link_libraries(KF5KIOGui PRIVATE KF5::WindowSystem)
endif()

# Headers prefixed with KIO/
ecm_generate_headers(KIOGui_CamelCase_HEADERS
  HEADER_NAMES
  ApplicationLauncherJob
  CommandLauncherJob
  FavIconRequestJob
  OpenUrlJob
  OpenWithHandlerInterface
  ThumbnailCreator

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_gui_HEADERS
)

# Headers not prefixed with KIO/
ecm_generate_headers(KIOGui_HEADERS
  HEADER_NAMES
  KCoreUrlNavigator
  KEMailClientLauncherJob
  KTerminalLauncherJob

  REQUIRED_HEADERS KIOGui_HEADERS
)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.101.0)
    list(APPEND KIOGui_CamelCase_HEADERS compatheaders/KCoreUrlNavigator)
    list(APPEND KIO_namespaced_gui_HEADERS compatheaders/kcoreurlnavigator.h)
endif()

install(FILES ${KIOGui_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui/KIO COMPONENT Devel)

install(TARGETS KF5KIOGui EXPORT KF5KIOTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIO_namespaced_gui_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui/kio COMPONENT Devel)

install(FILES
  ${KIOGui_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiogui_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOGui_QCH_SOURCES ${KIOGui_HEADERS} ${KIO_namespaced_gui_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOGui LIB_NAME KF5KIOGui DEPS "KIOCore" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
