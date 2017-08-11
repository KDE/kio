add_subdirectory(searchproviders)

set(kuriikwsfilter_PART_SRCS kuriikwsfiltereng.cpp kuriikwsfilter.cpp searchprovider.cpp)

ki18n_wrap_ui(kuriikwsfilter_PART_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

add_library(kuriikwsfilter MODULE ${kuriikwsfilter_PART_SRCS})

target_link_libraries(kuriikwsfilter
   KF5::ConfigWidgets  # KCModule
   KF5::KIOWidgets     # KUriFilter
   KF5::I18n
   Qt5::DBus
)

set_target_properties(kuriikwsfilter PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/urifilters")
install(TARGETS kuriikwsfilter DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/urifilters)

set(kurisearchfilter_PART_SRCS
   kurisearchfilter.cpp
   kuriikwsfiltereng.cpp
   ikwsopts.cpp
   searchproviderdlg.cpp
   searchprovider.cpp )

ki18n_wrap_ui(kurisearchfilter_PART_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

add_library(kurisearchfilter MODULE ${kurisearchfilter_PART_SRCS})

target_link_libraries(kurisearchfilter
   KF5::ConfigWidgets  # KCModule
   KF5::KIOWidgets     # KUriFilter
   KF5::I18n
   Qt5::DBus
)

set_target_properties(kurisearchfilter PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/urifilters")
install(TARGETS kurisearchfilter DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/urifilters)

########### install files ###############

install( FILES kuriikwsfilter.desktop kurisearchfilter.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES searchprovider.desktop  DESTINATION  ${KDE_INSTALL_KSERVICETYPES5DIR} )
