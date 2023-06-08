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

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.ikws
    OLD_CATEGORY_NAMES org.kde.kurifilter-ikws
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter IKWS (KIO)"
    EXPORT KIO
)

target_link_libraries(kuriikwsfilter
    KF6::KIOWidgets     # KUriFilter
    KF6::I18n
    Qt6::DBus
)

kcoreaddons_add_plugin(kurisearchfilter INSTALL_NAMESPACE "kf6/urifilters")

target_sources(kurisearchfilter PRIVATE
   kurisearchfilter.cpp
   kuriikwsfiltereng.cpp
   searchprovider.cpp
   searchproviderregistry.cpp
)

target_link_libraries(kurisearchfilter
   KF6::KIOWidgets     # KUriFilter
   KF6::I18n
   Qt6::DBus
)
