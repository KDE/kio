configure_file(config-kiogui.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiogui.h)

add_library(KF6KIOGui)
add_library(KF6::KIOGui ALIAS KF6KIOGui)

set_target_properties(KF6KIOGui PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOGui
)

target_sources(KF6KIOGui PRIVATE
   applicationlauncherjob.cpp
   commandlauncherjob.cpp
   dndpopupmenuplugin.cpp
   faviconrequestjob.cpp
   kcoreurlnavigator.cpp
   openurljob.cpp
   openwithhandlerinterface.cpp
   openorexecutefileinterface.cpp
   kprocessrunner.cpp
   kterminallauncherjob.cpp
   kemailclientlauncherjob.cpp
   previewjob.cpp
   thumbnailcreator.cpp
   gpudetection.cpp
   kurifilter.cpp
   kurifilterplugin.cpp
   openfilemanagerwindowjob.cpp
   standardthumbnailjob.cpp
)

if (HAVE_QTDBUS)
    target_sources(KF6KIOGui PRIVATE dbusactivationrunner.cpp)
endif ()

if (HAVE_QTDBUS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEMD_DBUS_XMLS
        systemd/org.freedesktop.systemd1.Manager.xml
        systemd/org.freedesktop.systemd1.Unit.xml
        systemd/org.freedesktop.DBus.Properties.xml)

    set_source_files_properties(${SYSTEMD_DBUS_XMLS} PROPERTIES INCLUDE systemd/dbustypes.h)

    set(kiogui_dbus_SRCS)
    qt_add_dbus_interfaces(kiogui_dbus_SRCS ${SYSTEMD_DBUS_XMLS})

    target_sources(KF6KIOGui PRIVATE
        ${kiogui_dbus_SRCS}
        systemd/systemdprocessrunner.cpp
        systemd/scopedprocessrunner.cpp
    )
endif ()

ecm_qt_declare_logging_category(KF6KIOGui
    HEADER kiogui_debug.h
    IDENTIFIER KIO_GUI
    CATEGORY_NAME kf.kio.gui
    OLD_CATEGORY_NAMES kf5.kio.gui
    DESCRIPTION "KIOGui (KIO)"
    EXPORT KIO
)

ecm_qt_declare_logging_category(KF6KIOGui
    HEADER favicons_debug.h
    IDENTIFIER FAVICONS_LOG
    CATEGORY_NAME kf.kio.gui.favicons
    OLD_CATEGORY_NAMES kf5.kio.favicons
    DESCRIPTION "FavIcons (KIO)"
    EXPORT KIO
)

ecm_generate_export_header(KF6KIOGui
    BASE_NAME KIOGui
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    USE_VERSION_HEADER
    VERSION_BASE_NAME KIO
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF6KIOGui INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui>")

target_link_libraries(KF6KIOGui
  PUBLIC
    KF6::KIOCore
    KF6::ConfigCore
    KF6::Service
    Qt6::Gui
  PRIVATE
    KF6::Solid
    KF6::I18n
)

target_link_libraries(KF6KIOGui PRIVATE KF6::WindowSystem)

# Headers prefixed with KIO/
ecm_generate_headers(KIOGui_CamelCase_HEADERS
  HEADER_NAMES
  ApplicationLauncherJob
  CommandLauncherJob
  DndPopupMenuPlugin
  FavIconRequestJob
  OpenFileManagerWindowJob
  OpenUrlJob
  OpenWithHandlerInterface
  PreviewJob
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
  KUriFilter

  REQUIRED_HEADERS KIOGui_HEADERS
)

install(FILES ${KIOGui_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui/KIO COMPONENT Devel)

install(TARGETS KF6KIOGui EXPORT KF6KIOTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIO_namespaced_gui_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui/kio COMPONENT Devel)

install(FILES
  ${KIOGui_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiogui_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOGui COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOGui_QCH_SOURCES ${KIOGui_HEADERS} ${KIO_namespaced_gui_HEADERS} PARENT_SCOPE)

