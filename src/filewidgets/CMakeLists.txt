project(KIOFileWidgets)

find_package(KF5Bookmarks ${KF_DEP_VERSION} REQUIRED)
find_package(KF5XmlGui ${KF_DEP_VERSION} REQUIRED)

configure_file(config-kiofilewidgets.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiofilewidgets.h)

set(kiofilewidgets_SRCS
  kstatusbarofflineindicator.cpp
  kfilemetapreview.cpp
  kimagefilepreview.cpp
  kpreviewwidgetbase.cpp
  krecentdirs.cpp
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
  kurlnavigatorprotocolcombo.cpp
  kurlnavigatortogglebutton.cpp
  kurlnavigator.cpp
  kurlnavigatormenu.cpp
  kurlnavigatorpathselectoreventfilter.cpp


)

qt5_add_resources(kiofilewidgets_SRCS ../new_file_templates/templates.qrc)

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

ecm_qt_declare_logging_category(kiofilewidgets_SRCS
    HEADER kfilefiltercombo_debug.h
    IDENTIFIER KIO_KFILEWIDGETS_KFILEFILTERCOMBO
    CATEGORY_NAME kf.kio.filewidgets.kfilefiltercombo
    OLD_CATEGORY_NAMES kf5.kio.filewidgets.kfilefiltercombo
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KFileFilterCombo (KIO)"
    EXPORT KIO
)

add_library(KF5KIOFileWidgets ${kiofilewidgets_SRCS})
add_library(KF5::KIOFileWidgets ALIAS KF5KIOFileWidgets)
ecm_generate_export_header(KF5KIOFileWidgets
    BASE_NAME KIOFileWidgets
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 4.3 4.5 5.0 5.33 5.66 5.70 5.76 5.78
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KIO libs
# needs fixing of undeprecated API being still implemented using own deprecated API

target_include_directories(KF5KIOFileWidgets INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KIOFileWidgets>")

target_link_libraries(KF5KIOFileWidgets
  PUBLIC
    KF5::KIOWidgets
    KF5::Bookmarks    # in KFilePlacesModel's API
    KF5::ItemViews     # kdirsortfilterproxymodel
    KF5::XmlGui        # for KActionCollection, used by KFileWidget/KDirOperator
    KF5::Solid         # KFilePlacesModel/KFilePlacesView
  PRIVATE
    KF5::IconThemes   # KIconLoader
    KF5::I18n
)

set_target_properties(KF5KIOFileWidgets PROPERTIES VERSION ${KIO_VERSION_STRING}
                                                   SOVERSION ${KIO_SOVERSION}
                                                   EXPORT_NAME KIOFileWidgets
)

ecm_generate_headers(KIOFileWidgets_HEADERS
  HEADER_NAMES
  KAbstractViewAdapter
  KImageFilePreview
  KPreviewWidgetBase
  KRecentDirs
  KStatusBarOfflineIndicator
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

install(TARGETS KF5KIOFileWidgets EXPORT KF5KIOTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${KIOFileWidgets_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiofilewidgets_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOFileWidgets COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOFileWidgets_QCH_SOURCES ${KIOFileWidgets_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOFileWidgets LIB_NAME KF5KIOFileWidgets DEPS "KIOWidgets KBookmarks KXmlGui Solid" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOFileWidgets)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
