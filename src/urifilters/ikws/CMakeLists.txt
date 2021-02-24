add_subdirectory(searchproviders)

####

set(kuriikwsfilter_SRCS
   kuriikwsfiltereng.cpp
   kuriikwsfilter.cpp
   searchprovider.cpp
   searchproviderregistry.cpp
)

ki18n_wrap_ui(kuriikwsfilter_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

kcoreaddons_add_plugin(kuriikwsfilter INSTALL_NAMESPACE "kf5/urifilters" JSON kuriikwsfilter.json SOURCES ${kuriikwsfilter_SRCS})
ecm_generate_qloggingcategories_source(kuriikwsfilter
    HEADER kuriikwsfilter_debug.h
    GROUP KURIIKWSFILTER
)

target_link_libraries(kuriikwsfilter
   KF5::ConfigWidgets  # KCModule
   KF5::KIOWidgets     # KUriFilter
   KF5::I18n
   Qt5::DBus
)

#####

set(kurisearchfilter_SRCS
   kurisearchfilter.cpp
   kuriikwsfiltereng.cpp
   ikwsopts.cpp
   searchproviderdlg.cpp
   searchprovider.cpp
   searchproviderregistry.cpp
)

ki18n_wrap_ui(kurisearchfilter_SRCS ikwsopts_ui.ui searchproviderdlg_ui.ui)

kcoreaddons_add_plugin(kurisearchfilter INSTALL_NAMESPACE "kf5/urifilters" JSON kurisearchfilter.json SOURCES ${kurisearchfilter_SRCS})
ecm_generate_qloggingcategories_source(kurisearchfilter
    HEADER kurisearchfilter_debug.h
    GROUP KURIIKWSFILTER
)

target_link_libraries(kurisearchfilter
   KF5::ConfigWidgets  # KCModule
   KF5::KIOWidgets     # KUriFilter
   KF5::I18n
   Qt5::DBus
)

########### install files ###############

install( FILES searchprovider.desktop  DESTINATION  ${KDE_INSTALL_KSERVICETYPES5DIR} )
