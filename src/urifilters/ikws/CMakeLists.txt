add_subdirectory(searchproviders)

####

add_library(kuriikwsfiltereng_common kuriikwsfiltereng.cpp searchprovider.cpp searchproviderregistry.cpp)
generate_export_header(kuriikwsfiltereng_common)
target_link_libraries(kuriikwsfiltereng_common PUBLIC KF6::KIOGui)

kcoreaddons_add_plugin(kuriikwsfilter SOURCES kuriikwsfilter.cpp INSTALL_NAMESPACE "kf6/urifilters")

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.ikws
    OLD_CATEGORY_NAMES org.kde.kurifilter-ikws
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter IKWS (KIO)"
    EXPORT KIO
)

target_link_libraries(kuriikwsfilter
    KF6::I18n
    Qt6::DBus
    kuriikwsfiltereng_common
)

kcoreaddons_add_plugin(kurisearchfilter SOURCES kurisearchfilter.cpp INSTALL_NAMESPACE "kf6/urifilters")

target_link_libraries(kurisearchfilter
   Qt6::DBus
   kuriikwsfiltereng_common
)
install(TARGETS kuriikwsfiltereng_common ${KF_INSTALL_TARGETS_DEFAULT_ARGS})
