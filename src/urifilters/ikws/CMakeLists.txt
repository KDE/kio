add_subdirectory(searchproviders)

####

kcoreaddons_add_plugin(kuriikwsfilter
    INSTALL_NAMESPACE "kf6/urifilters"
)

target_sources(kuriikwsfilter PRIVATE
   kuriikwsfiltereng.cpp
   kuriikwsfilter.cpp
   searchprovider.cpp
   searchproviderregistry.cpp
)

ki18n_wrap_ui(kuriikwsfilter
    ikwsopts_ui.ui
    searchproviderdlg_ui.ui
)

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.ikws
    OLD_CATEGORY_NAMES org.kde.kurifilter-ikws
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter IKWS (KIO)"
    EXPORT KIO
)

target_link_libraries(kuriikwsfilter
    PRIVATE
        KF6::ConfigWidgets  # KCModule
        KF6::KIOWidgets     # KUriFilter
        KF6::I18n
        Qt6::Core5Compat # QTextCodec
        Qt6::DBus
)

#####

kcoreaddons_add_plugin(kurisearchfilter
    INSTALL_NAMESPACE "kf6/urifilters"
)

target_sources(kurisearchfilter PRIVATE
   kurisearchfilter.cpp
   kuriikwsfiltereng.cpp
   ikwsopts.cpp
   searchproviderdlg.cpp
   searchprovider.cpp
   searchproviderregistry.cpp
)

ki18n_wrap_ui(kurisearchfilter ikwsopts_ui.ui searchproviderdlg_ui.ui)

target_link_libraries(kurisearchfilter
   KF6::ConfigWidgets  # KCModule
   KF6::KIOWidgets     # KUriFilter
   KF6::I18n
   Qt6::Core5Compat # QTextCodec
   Qt6::DBus
)

########### install files ###############

install( FILES searchprovider.desktop  DESTINATION  ${KDE_INSTALL_KSERVICETYPESDIR} )
