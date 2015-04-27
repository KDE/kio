project(KIOFileWidgets)

find_package(KF5Bookmarks ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5XmlGui ${KF5_DEP_VERSION} REQUIRED)

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
  kdirsortfilterproxymodel.cpp #used in combination with kdirmodel.cpp
  kencodingfiledialog.cpp
  kfilebookmarkhandler.cpp
  kfilecopytomenu.cpp
  kfilefiltercombo.cpp
  kfilewidget.cpp
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

)

add_library(KF5KIOFileWidgets ${kiofilewidgets_SRCS})
generate_export_header(KF5KIOFileWidgets BASE_NAME KIOFileWidgets)
add_library(KF5::KIOFileWidgets ALIAS KF5KIOFileWidgets)

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
  KFileFilterCombo
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

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOFileWidgets LIB_NAME KF5KIOFileWidgets DEPS "KIOWidgets KBookmarks KXmlGui Solid" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOFileWidgets)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
