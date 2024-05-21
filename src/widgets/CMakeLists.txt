#include (ConfigureChecks.cmake)

set_package_properties(ACL PROPERTIES DESCRIPTION "LibACL" URL "ftp://oss.sgi.com/projects/xfs/cmd_tars"
                       TYPE RECOMMENDED PURPOSE "Support for manipulating access control lists")
configure_file(config-kiowidgets.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiowidgets.h)

add_library(KF6KIOWidgets)
add_library(KF6::KIOWidgets ALIAS KF6KIOWidgets)

set_target_properties(KF6KIOWidgets PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOWidgets
)

set(kiowidgets_dbus_SRCS)
if(HAVE_QTDBUS)
    qt_add_dbus_adaptor(kiowidgets_dbus_SRCS org.kde.kio.FileUndoManager.xml fileundomanager_p.h KIO::FileUndoManagerPrivate fileundomanager_adaptor KIOFileUndoManagerAdaptor)
    qt_add_dbus_interface(kiowidgets_dbus_SRCS org.kde.kuiserver.xml kuiserver_interface)
endif()


target_sources(KF6KIOWidgets PRIVATE
  kacleditwidget.cpp
  kurlrequesterdialog.cpp
  kurlcombobox.cpp
  kfileitemactions.cpp
  imagefilter.cpp
  kopenwithdialog.cpp
  kfile.cpp
  pastedialog.cpp
  clipboardupdater.cpp
  kabstractfileitemactionplugin.cpp
  kurlrequester.cpp
  kshellcompletion.cpp
  kurlcompletion.cpp
  renamedialog.cpp
  ksslcertificatebox.cpp
  ksslinfodialog.cpp
  skipdialog.cpp
  jobuidelegate.cpp
  kdirlister.cpp
  executablefileopendialog.cpp
  kurifiltersearchprovideractions.cpp
  widgetsuntrustedprogramhandler.cpp
  widgetsopenwithhandler.cpp
  widgetsopenorexecutefilehandler.cpp
  widgetsaskuseractionhandler.cpp
  deleteortrashjob.cpp
  fileundomanager.cpp
  paste.cpp
  pastejob.cpp
  joburlcache.cpp
  kdirmodel.cpp
  dropjob.cpp
  kbuildsycocaprogressdialog.cpp
  renamefiledialog.cpp
  kfileitemdelegate.cpp
  delegateanimationhandler.cpp
  kpropertiesdialog.cpp
  kpropertiesdialogplugin.cpp
  kpropertiesdialogbuiltin_p.cpp
  sslui.cpp
)

if (HAVE_QTDBUS)
    target_sources(KF6KIOWidgets PRIVATE
        kdynamicjobtracker.cpp
        ${kiowidgets_dbus_SRCS}
    )
endif()

ecm_qt_declare_logging_category(KF6KIOWidgets
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

ki18n_wrap_ui(KF6KIOWidgets
  checksumswidget.ui
  certificateparty.ui
  sslinfo.ui
  kpropertiesdesktopadvbase.ui
  kpropertiesdesktopbase.ui
  kfilepropspluginwidget.ui
)

ecm_generate_export_header(KF6KIOWidgets
    BASE_NAME KIOWidgets
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    USE_VERSION_HEADER
    VERSION_BASE_NAME KIO
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.0
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF6KIOWidgets INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOWidgets>")

target_link_libraries(KF6KIOWidgets
  PUBLIC
    KF6::KIOGui
    KF6::KIOCore
    KF6::JobWidgets
    KF6::Service
    KF6::Solid
    Qt6::Network  # SSL
    KF6::Completion    # KUrlCompletion uses KCompletion
    KF6::WidgetsAddons # keditlistwidget
  PRIVATE
    Qt6::Concurrent
    KF6::I18n
    KF6::ConfigGui
    KF6::GuiAddons # KIconUtils
    KF6::IconThemes   # KIconLoader
    KF6::IconWidgets  # KIconButton
    KF6::WindowSystem # KStartupInfo
    KF6::ColorScheme
)
if (HAVE_QTDBUS)
    target_link_libraries(KF6KIOWidgets PRIVATE ${DBUS_LIB})
endif()

if(ACL_FOUND)
    target_link_libraries(KF6KIOWidgets PRIVATE ${ACL_LIBS})
endif()

# Headers not prefixed with KIO/
ecm_generate_headers(KIOWidgets_HEADERS
  HEADER_NAMES
  KPropertiesDialog
  KPropertiesDialogPlugin
  KUrlRequesterDialog
  KUrlComboBox
  KFileItemActions
  KFileItemDelegate
  KOpenWithDialog
  KAbstractFileItemActionPlugin
  KBuildSycocaProgressDialog
  KFile
  KUrlRequester
  KSslCertificateBox
  KSslInfoDialog
  KDirLister
  KDirModel
  KShellCompletion
  KUrlCompletion

  REQUIRED_HEADERS KIOWidgets_HEADERS
)

# Headers prefixed with KIO/
ecm_generate_headers(KIOWidgets_CamelCase_HEADERS
  HEADER_NAMES
  DeleteOrTrashJob
  SslUi
  DropJob
  PasteJob
  RenameDialog
  SkipDialog
  JobUiDelegate
  FileUndoManager
  Paste
  KUriFilterSearchProviderActions # KF6: fix and move to non-KIO prefixed install folder
  RenameFileDialog
  WidgetsAskUserActionHandler

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_widgets_HEADERS
)
install(FILES ${KIOWidgets_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOWidgets/KIO COMPONENT Devel)

install(TARGETS KF6KIOWidgets EXPORT KF6KIOTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIO_namespaced_widgets_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOWidgets/kio COMPONENT Devel)

install(FILES
  ${KIOWidgets_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiowidgets_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOWidgets COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOWidgets_QCH_SOURCES ${KIOWidgets_HEADERS} ${KIO_namespaced_widgets_HEADERS} PARENT_SCOPE)

