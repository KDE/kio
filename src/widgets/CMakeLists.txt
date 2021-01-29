project(KIOWidgets)

#include (ConfigureChecks.cmake)

set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL" URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED PURPOSE "Support for manipulating access control lists")
configure_file(config-kiowidgets.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiowidgets.h)

set(kiowidgets_SRCS
  accessmanager.cpp
  accessmanagerreply_p.cpp
  fileundomanager.cpp
  kacleditwidget.cpp
  kpropertiesdialog.cpp
  kurlrequesterdialog.cpp
  kurlcombobox.cpp
  kfileitemactions.cpp
  delegateanimationhandler.cpp
  imagefilter.cpp
  kfileitemdelegate.cpp
  kdesktopfileactions.cpp
  kopenwithdialog.cpp
  kfile.cpp
  pastedialog.cpp
  paste.cpp
  clipboardupdater.cpp
  kabstractfileitemactionplugin.cpp
  koverlayiconplugin.cpp
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
  dropjob.cpp
  openfilemanagerwindowjob.cpp
  pastejob.cpp
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
  executablefileopendialog.cpp
  dndpopupmenuplugin.cpp
  kurifiltersearchprovideractions.cpp
  renamefiledialog.cpp
  widgetsuntrustedprogramhandler.cpp
  widgetsopenwithhandler.cpp
  widgetsopenorexecutefilehandler.cpp
  widgetsaskuseractionhandler.cpp
)
if (NOT WIN32)
  list(APPEND kiowidgets_SRCS
    kautomount.cpp
  )
endif()

ecm_qt_declare_logging_category(kiowidgets_SRCS
    HEADER kio_widgets_debug.h
    IDENTIFIER KIO_WIDGETS
    CATEGORY_NAME kf.kio.widgets
    OLD_CATEGORY_NAMES kf5.kio.widgets
    DESCRIPTION "KIOWidgets (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.widgets.kdirmodel
    OLD_CATEGORY_NAMES kf5.kio.kdirmodel
    DESCRIPTION "KDirModel (KIO)"
    EXPORT KIO
)

qt5_add_dbus_adaptor(kiowidgets_SRCS org.kde.kio.FileUndoManager.xml fileundomanager_p.h KIO::FileUndoManagerPrivate fileundomanager_adaptor KIOFileUndoManagerAdaptor)

qt5_add_dbus_interface(kiowidgets_SRCS org.kde.kuiserver.xml kuiserver_interface)

ki18n_wrap_ui(kiowidgets_SRCS
  checksumswidget.ui
  certificateparty.ui
  sslinfo.ui
  kpropertiesdesktopadvbase.ui
  kpropertiesdesktopbase.ui
)

add_library(KF5KIOWidgets ${kiowidgets_SRCS})
add_library(KF5::KIOWidgets ALIAS KF5KIOWidgets)
ecm_generate_export_header(KF5KIOWidgets
    BASE_NAME KIOWidgets
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 4.0 4.1 4.3 4.4 4.5 4.6 4.7 5.0 5.4 5.6 5.25 5.31 5.32 5.64 5.66 5.71 5.75 5.76 5.79
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KIO libs
# needs fixing of undeprecated API being still implemented using own deprecated API

target_include_directories(KF5KIOWidgets INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KIOWidgets>")

target_link_libraries(KF5KIOWidgets
  PUBLIC
    KF5::KIOGui
    KF5::KIOCore
    KF5::JobWidgets
    KF5::Service
    Qt5::Network        # SSL
    KF5::Completion    # KUrlCompletion uses KCompletion
    KF5::WidgetsAddons # keditlistwidget
  PRIVATE
    Qt5::Concurrent
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

# Headers not prefixed with KIO/
ecm_generate_headers(KIOWidgets_HEADERS
  HEADER_NAMES
  KPropertiesDialog
  KUrlRequesterDialog
  KUrlComboBox
  KFileItemActions
  KFileItemDelegate
  KAutoMount
  KDesktopFileActions
  KOpenWithDialog
  KAbstractFileItemActionPlugin
  KOverlayIconPlugin
  KBuildSycocaProgressDialog
  KFile
  KUrlRequester
  KRun
  KUrlPixmapProvider
  KSslCertificateBox
  KSslInfoDialog
  KDirLister
  KDirModel
  KShellCompletion
  KUrlCompletion
  KUriFilter

  REQUIRED_HEADERS KIOWidgets_HEADERS
)

# Headers prefixed with KIO/
ecm_generate_headers(KIOWidgets_CamelCase_HEADERS
  HEADER_NAMES
  AccessManager
  SslUi
  ThumbSequenceCreator
  ThumbCreator
  DropJob
  DndPopupMenuPlugin
  OpenFileManagerWindowJob
  PasteJob
  PreviewJob
  RenameDialog
  SkipDialog
  JobUiDelegate
  FileUndoManager
  Paste
  PixmapLoader
  KUriFilterSearchProviderActions # KF6: fix and move to non-KIO prefixed install folder
  RenameFileDialog

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_widgets_HEADERS
)
install(FILES ${KIOWidgets_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOWidgets/KIO COMPONENT Devel)

install(TARGETS KF5KIOWidgets EXPORT KF5KIOTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
   org.kde.kio.FileUndoManager.xml
   DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
   RENAME kf5_org.kde.kio.FileUndoManager.xml)

install(FILES
  ${KIO_namespaced_widgets_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOWidgets/kio COMPONENT Devel)

install(FILES
  ${KIOWidgets_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiowidgets_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOWidgets COMPONENT Devel)

install(FILES
  kfileitemactionplugin.desktop
  kpropertiesdialogplugin.desktop
  kurifilterplugin.desktop
  konqpopupmenuplugin.desktop
  kiodndpopupmenuplugin.desktop
   DESTINATION  ${KDE_INSTALL_KSERVICETYPES5DIR} )

# make available to ecm_add_qch in parent folder
set(KIOWidgets_QCH_SOURCES ${KIOWidgets_HEADERS} ${KIO_namespaced_widgets_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOWidgets LIB_NAME KF5KIOWidgets DEPS "KIOGui KIOCore KBookmarks KXmlGui Solid" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOWidgets)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
