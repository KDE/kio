find_package(KF6Bookmarks ${KF_DEP_VERSION} REQUIRED)

configure_file(config-kiofilewidgets.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiofilewidgets.h)

add_library(KF6KIOFileWidgets)
add_library(KF6::KIOFileWidgets ALIAS KF6KIOFileWidgets)

set_target_properties(KF6KIOFileWidgets PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOFileWidgets
)

target_sources(KF6KIOFileWidgets PRIVATE
  kfilemetapreview.cpp
  kimagefilepreview.cpp
  kpreviewwidgetbase.cpp
  defaultviewadapter.cpp

  kdiroperator.cpp
  kdiroperatordetailview.cpp
  kdiroperatoriconview.cpp
  kdirsortfilterproxymodel.cpp #used in combination with kdirmodel.cpp
  kencodingfiledialog.cpp
  kfilebookmarkhandler.cpp
  kfilecopytomenu.cpp
  kfilecustomdialog.cpp
  kfilefiltercombo.cpp
  kfilewidget.cpp
  kfilewidgetdocktitlebar.cpp
  kfileplacesitem.cpp
  kfileplacesmodel.cpp
  kfileplacesview.cpp
  kfileplaceeditdialog.cpp
  kfilepreviewgenerator.cpp
  knameandurlinputdialog.cpp
  knewfilemenu.cpp
  kurlnavigatordropdownbutton.cpp
  kurlnavigatorbuttonbase.cpp
  kurlnavigatorbutton.cpp
  kurlnavigatorplacesselector.cpp
  kurlnavigatorschemecombo.cpp
  kurlnavigatortogglebutton.cpp
  kurlnavigator.cpp
  kurlnavigatormenu.cpp
  kurlnavigatorpathselectoreventfilter.cpp

  ../new_file_templates/templates.qrc
)


ecm_qt_export_logging_category(
    IDENTIFIER KFILEWIDGETS_LOG
    CATEGORY_NAME kf.kio.filewidgets
    OLD_CATEGORY_NAMES kf5.kio.filewidgets
    DESCRIPTION "KFileWidgets (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_KFILEWIDGETS_FW
    CATEGORY_NAME kf.kio.filewidgets.kfilewidget
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilewidget
    DEFAULT_SEVERITY Info
    DESCRIPTION "KFileWidgets (KIO)"
    EXPORT KIO
)

ecm_qt_declare_logging_category(KF6KIOFileWidgets
    HEADER kfilefiltercombo_debug.h
    IDENTIFIER KIO_KFILEWIDGETS_KFILEFILTERCOMBO
    CATEGORY_NAME kf.kio.filewidgets.kfilefiltercombo
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilefiltercombo
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KFileFilterCombo (KIO)"
    EXPORT KIO
)

ecm_qt_declare_logging_category(KF6KIOFileWidgets
    HEADER kfilewidgets_debug.h
    IDENTIFIER KFILEWIDGETS_LOG
    CATEGORY_NAME kf.kio.filewidgets.kfilewidgets
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilewidgets
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KFileWidgets (KIO)"
    EXPORT KIO
)

ecm_generate_export_header(KF6KIOFileWidgets
    BASE_NAME KIOFileWidgets
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    USE_VERSION_HEADER
    VERSION_BASE_NAME KIO
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 6.3
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF6KIOFileWidgets INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOFileWidgets>")

target_link_libraries(KF6KIOFileWidgets
  PUBLIC
    KF6::KIOWidgets
    KF6::Bookmarks    # in KFilePlacesModel's API
    KF6::ItemViews     # kdirsortfilterproxymodel
    KF6::Solid         # KFilePlacesModel/KFilePlacesView
  PRIVATE
    Qt6::Core5Compat
    KF6::GuiAddons    # KIconUtils
    KF6::IconThemes   # KIconLoader
    KF6::IconWidgets   # KIconButton
    KF6::I18n
    KF6::ConfigGui
    KF6::ConfigWidgets
    KF6::BookmarksWidgets
)

ecm_generate_headers(KIOFileWidgets_HEADERS
  HEADER_NAMES
  KAbstractViewAdapter
  KImageFilePreview
  KPreviewWidgetBase
  KDirOperator
  KDirSortFilterProxyModel
  KFileCopyToMenu
  KFileCustomDialog
  KFileFilterCombo
  KFilePlaceEditDialog
  KFilePlacesModel
  KFilePlacesView
  KFilePreviewGenerator
  KFileWidget
  KUrlNavigator
  KNewFileMenu
  KNameAndUrlInputDialog
  KEncodingFileDialog

  REQUIRED_HEADERS KIOFileWidgets_HEADERS
)

install(TARGETS KF6KIOFileWidgets EXPORT KF6KIOTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIOFileWidgets_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiofilewidgets_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOFileWidgets COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOFileWidgets_QCH_SOURCES ${KIOFileWidgets_HEADERS} PARENT_SCOPE)

