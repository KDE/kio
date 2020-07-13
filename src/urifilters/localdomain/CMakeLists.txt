set(localdomainurifilter_SRCS localdomainurifilter.cpp )

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.localdomain
    OLD_CATEGORY_NAMES org.kde.kurifilter-localdomain
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter Local Domain (KIO)"
    EXPORT KIO
)

kcoreaddons_add_plugin(localdomainurifilter INSTALL_NAMESPACE "kf5/urifilters" JSON localdomainurifilter.json SOURCES ${localdomainurifilter_SRCS})

target_link_libraries(localdomainurifilter KF5::KIOWidgets)

