kcoreaddons_add_plugin(localdomainurifilter SOURCES localdomainurifilter.cpp INSTALL_NAMESPACE "kf6/urifilters")

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.localdomain
    OLD_CATEGORY_NAMES org.kde.kurifilter-localdomain
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter Local Domain (KIO)"
    EXPORT KIO
)

target_link_libraries(localdomainurifilter KF6::KIOGui)

