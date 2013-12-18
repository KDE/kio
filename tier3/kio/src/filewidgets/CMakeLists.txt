project(KIOFileWidgets)

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
  kfilefiltercombo.cpp
  kfilewidget.cpp
  kfileplacesitem.cpp
  kfileplacesmodel.cpp
  kfileplacessharedbookmarks.cpp
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

target_include_directories(KF5KIOFileWidgets PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>" # kio/global.h
)

target_link_libraries(KF5KIOFileWidgets
  PUBLIC
    KF5::KIOWidgets
    KF5::Bookmarks    # in KFilePlacesModel's API
    KF5::XmlGui        # for KActionCollection, used by KFileWidget/KDirOperator
    KF5::Solid         # KFilePlacesModel/KFilePlacesView
  PRIVATE
    KF5::IconThemes   # KIconLoader
    KF5::I18n
    KF5::ItemViews     # kdirsortfilterproxymodel
)

set_target_properties(KF5KIOFileWidgets PROPERTIES VERSION ${KIO_VERSION_STRING}
                                                   SOVERSION ${KIO_SOVERSION}
                                                   EXPORT_NAME KIOFileWidgets
)

install(TARGETS KF5KIOFileWidgets EXPORT KF5KIOTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install( FILES
  kimagefilepreview.h
  kpreviewwidgetbase.h
  krecentdirs.h
  kstatusbarofflineindicator.h

  kabstractviewadapter.h
  kdiroperator.h
  kdirsortfilterproxymodel.h
  kencodingfiledialog.h
  kfilefiltercombo.h
  kfileplacesmodel.h
  kfileplacesview.h
  kfilepreviewgenerator.h
  kfilewidget.h
  kurlnavigator.h
  knewfilemenu.h
  knameandurlinputdialog.h

  DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kiofilewidgets_export.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/kio
  COMPONENT Devel)
