project(KIOWidgets)

#include (ConfigureChecks.cmake)

# Device desktop files aren't really used anymore
set(KIO_NO_SOLID TRUE)


find_package(ACL)
set(HAVE_LIBACL ${ACL_FOUND})
set(HAVE_POSIX_ACL ${ACL_FOUND})

set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL" URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED PURPOSE "Support for manipulating access control lists")
check_symbol_exists(getgrouplist    "unistd.h;grp.h"           HAVE_GETGROUPLIST)
configure_file(config-kiowidgets.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiowidgets.h)
configure_file(config-getgrouplist.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-getgrouplist.h)


set(kiowidgets_SRCS
  accessmanager.cpp
  accessmanagerreply_p.cpp
  fileundomanager.cpp
  kacleditwidget.cpp
  kpropertiesdialog.cpp
  getgrouplist-fake.c
  kurlrequesterdialog.cpp
  kurlcombobox.cpp
  kfileitemactions.cpp
  delegateanimationhandler.cpp
  imagefilter.cpp
  kfileitemdelegate.cpp
  kautomount.cpp
  kdesktopfileactions.cpp
  kopenwithdialog.cpp
  kfile.cpp
  pastedialog.cpp
  paste.cpp
  clipboardupdater.cpp
  kabstractfileitemactionplugin.cpp
  kbuildsycocaprogressdialog.cpp
  kurlrequester.cpp
  krun.cpp
  sslui.cpp
  kurlpixmapprovider.cpp
  pixmaploader.cpp
  thumbsequencecreator.cpp
  thumbcreator.cpp
  kshellcompletion.cpp
  kurlcompletion.cpp
  kurifilter.cpp
  previewjob.cpp
  renamedialog.cpp
  ksslcertificatebox.cpp
  kdynamicjobtracker.cpp
  ksslinfodialog.cpp
  joburlcache.cpp
  skipdialog.cpp
  jobuidelegate.cpp
  kdirlister.cpp
  kdirmodel.cpp
)
if (WIN32)
  LIST(APPEND kiowidgets_SRCS
    krun_win.cpp
  )
endif()

qt5_add_dbus_adaptor(kiowidgets_SRCS org.kde.kio.FileUndoManager.xml fileundomanager_p.h KIO::FileUndoManagerPrivate fileundomanager_adaptor KIOFileUndoManagerAdaptor)

qt5_add_dbus_interface(kiowidgets_SRCS org.kde.kuiserver.xml kuiserver_interface)

qt5_wrap_ui(kiowidgets_SRCS
  certificateparty.ui
  sslinfo.ui
  kpropertiesdesktopadvbase.ui
  kpropertiesdesktopbase.ui
)

qt5_add_resources(kiowidgets_SRCS kacleditwidget.qrc)

add_library(KF5KIOWidgets ${kiowidgets_SRCS})
generate_export_header(KF5KIOWidgets BASE_NAME KIOWidgets)
add_library(KF5::KIOWidgets ALIAS KF5KIOWidgets)

target_include_directories(KF5KIOWidgets PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>" # kio/global.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include/kio>" # krun.h
)

target_link_libraries(KF5KIOWidgets
  PUBLIC
    KF5::KIOCore
    KF5::JobWidgets
    Qt5::Network        # SSL
    KF5::Completion    # KUrlCompletion uses KCompletion
    KF5::WidgetsAddons # keditlistwidget
  PRIVATE
    Qt5::DBus
    KF5::I18n
    KF5::IconThemes   # KIconLoader
    KF5::WindowSystem # KStartupInfo
    KF5::ConfigWidgets # KColorScheme
)

if(ACL_FOUND)
   target_link_libraries(KF5KIOWidgets PRIVATE ${ACL_LIBS})
endif()

set_target_properties(KF5KIOWidgets PROPERTIES VERSION ${KIO_VERSION_STRING}
                                               SOVERSION ${KIO_SOVERSION}
                                               EXPORT_NAME KIOWidgets
)

install(TARGETS KF5KIOWidgets EXPORT KF5KIOTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
   org.kde.kio.FileUndoManager.xml
   DESTINATION ${DBUS_INTERFACES_INSTALL_DIR})

install( FILES
  pastedialog.h
  paste.h
  accessmanager.h
  sslui.h
  pixmaploader.h
  thumbsequencecreator.h
  thumbcreator.h
  previewjob.h
  renamedialog.h
  skipdialog.h
  jobuidelegate.h
  fileundomanager.h

   ${CMAKE_CURRENT_BINARY_DIR}/kiowidgets_export.h
   DESTINATION ${INCLUDE_INSTALL_DIR}/kio COMPONENT Devel)

install( FILES
  kacleditwidget.h
  kpropertiesdialog.h
  kpropertiesdialog.h
  kurlrequesterdialog.h
  kurlcombobox.h
  kfileitemactions.h
  kfileitemdelegate.h
  kautomount.h
  kdesktopfileactions.h
  kopenwithdialog.h
  kabstractfileitemactionplugin.h
  kbuildsycocaprogressdialog.h
  kfile.h
  kurlrequester.h
  krun.h
  kurlpixmapprovider.h
  ksslcertificatebox.h
  ksslinfodialog.h
  kdirlister.h
  kdirmodel.h
  kshellcompletion.h
  kurlcompletion.h
  kurifilter.h
   DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)

install( FILES
  kfileitemactionplugin.desktop
  kpropertiesdialogplugin.desktop
  kurifilterplugin.desktop
   DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
