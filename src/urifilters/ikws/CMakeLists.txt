add_subdirectory(searchproviders)

add_library(kuriikwsfiltereng_private kuriikwsfiltereng.cpp searchprovider.cpp searchproviderregistry.cpp)
generate_export_header(kuriikwsfiltereng_private) # We need this to be a shared lib, because otherwise we have duplicate Q_GLOBAL_STATICs
target_link_libraries(kuriikwsfiltereng_private PUBLIC KF6::KIOGui)
install(TARGETS kuriikwsfiltereng_private ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

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
    kuriikwsfiltereng_private
)

kcoreaddons_add_plugin(kurisearchfilter SOURCES kurisearchfilter.cpp INSTALL_NAMESPACE "kf6/urifilters")

target_link_libraries(kurisearchfilter
   kuriikwsfiltereng_private
)
