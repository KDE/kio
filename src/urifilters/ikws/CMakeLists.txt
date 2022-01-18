add_subdirectory(searchproviders)

####

kcoreaddons_add_plugin(kuriikwsfilter
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/urifilters"
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
        KF5::ConfigWidgets  # KCModule
        KF5::KIOWidgets     # KUriFilter
        KF5::I18n
        Qt${QT_MAJOR_VERSION}::DBus
)

if (TARGET Qt6::Core5Compat)
    target_link_libraries(kuriikwsfilter PRIVATE Qt6::Core5Compat) # QTextCodec
endif()

#####

kcoreaddons_add_plugin(kurisearchfilter
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/urifilters"
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
   KF5::ConfigWidgets  # KCModule
   KF5::KIOWidgets     # KUriFilter
   KF5::I18n
   Qt${QT_MAJOR_VERSION}::DBus
)

########### install files ###############

install( FILES searchprovider.desktop  DESTINATION  ${KDE_INSTALL_KSERVICETYPESDIR} )
