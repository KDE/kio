add_subdirectory(searchproviders) 

set(kuriikwsfilter_PART_SRCS kuriikwsfiltereng.cpp kuriikwsfilter.cpp searchprovider.cpp)

qt5_wrap_ui(kuriikwsfilter_PART_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

add_library(kuriikwsfilter MODULE ${kuriikwsfilter_PART_SRCS})

target_link_libraries(kuriikwsfilter KF5::ConfigWidgets KF5::KIOWidgets KF5::I18n Qt5::DBus)

install(TARGETS kuriikwsfilter DESTINATION ${PLUGIN_INSTALL_DIR})

set(kurisearchfilter_PART_SRCS 
   kurisearchfilter.cpp 
   kuriikwsfiltereng.cpp 
   ikwsopts.cpp 
   searchproviderdlg.cpp 
   searchprovider.cpp )

qt5_wrap_ui(kurisearchfilter_PART_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

add_library(kurisearchfilter MODULE ${kurisearchfilter_PART_SRCS})

target_link_libraries(kurisearchfilter KF5::I18n KF5::KIOWidgets KF5::ConfigWidgets Qt5::DBus)

install(TARGETS kurisearchfilter DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install( FILES kuriikwsfilter.desktop kurisearchfilter.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES searchprovider.desktop  DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
