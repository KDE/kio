configure_file(config-kiogui.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiogui.h)

set(kiogui_SRCS
   applicationlauncherjob.cpp
   commandlauncherjob.cpp
   faviconrequestjob.cpp
   openurljob.cpp
   openwithhandlerinterface.cpp
   openorexecutefileinterface.cpp
   kprocessrunner.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEMD_DBUS_XMLS
        systemd/org.freedesktop.systemd1.Manager.xml
        systemd/org.freedesktop.systemd1.Unit.xml
        systemd/org.freedesktop.DBus.Properties.xml)

    set_source_files_properties(${SYSTEMD_DBUS_XMLS} PROPERTIES INCLUDE systemd/dbustypes.h)

    qt5_add_dbus_interfaces(kiogui_SRCS ${SYSTEMD_DBUS_XMLS})

    list(APPEND kiogui_SRCS systemd/systemdprocessrunner.cpp systemd/scopedprocessrunner.cpp)
endif ()

ecm_qt_declare_logging_category(kiogui_SRCS
    HEADER kiogui_debug.h
    IDENTIFIER KIO_GUI
    CATEGORY_NAME kf.kio.gui
    OLD_CATEGORY_NAMES kf5.kio.gui
    DESCRIPTION "KIOGui (KIO)"
    EXPORT KIO
)

ecm_qt_declare_logging_category(kiogui_SRCS
    HEADER favicons_debug.h
    IDENTIFIER FAVICONS_LOG
    CATEGORY_NAME kf.kio.gui.favicons
    OLD_CATEGORY_NAMES kf5.kio.favicons
    DESCRIPTION "FavIcons (KIO)"
    EXPORT KIO
)

add_library(KF5KIOGui ${kiogui_SRCS})
generate_export_header(KF5KIOGui BASE_NAME KIOGui)
add_library(KF5::KIOGui ALIAS KF5KIOGui)

target_include_directories(KF5KIOGui INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KIOGui>")

target_link_libraries(KF5KIOGui
  PUBLIC
    KF5::KIOCore
    KF5::WindowSystem
    Qt5::Gui
  PRIVATE
    KF5::I18n
)

set_target_properties(KF5KIOGui PROPERTIES VERSION ${KIO_VERSION_STRING}
                                               SOVERSION ${KIO_SOVERSION}
                                               EXPORT_NAME KIOGui
)

# Headers prefixed with KIO/
ecm_generate_headers(KIOGui_CamelCase_HEADERS
  HEADER_NAMES
  ApplicationLauncherJob
  CommandLauncherJob
  FavIconRequestJob
  OpenUrlJob

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_gui_HEADERS
)
install(FILES ${KIOGui_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOGui/KIO COMPONENT Devel)

install(TARGETS KF5KIOGui EXPORT KF5KIOTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIO_namespaced_gui_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOGui/kio COMPONENT Devel)

install(FILES
  ${KIOGui_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiogui_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOGui COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOGui_QCH_SOURCES ${KIOGui_HEADERS} ${KIO_namespaced_gui_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOGui LIB_NAME KF5KIOGui DEPS "KIOCore" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOGui)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
