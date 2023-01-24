kcoreaddons_add_plugin(kshorturifilter
    INSTALL_NAMESPACE "kf6/urifilters"
)

target_sources(kshorturifilter PRIVATE
    kshorturifilter.cpp
)

ecm_qt_export_logging_category(
    IDENTIFIER category
    CATEGORY_NAME kf.kio.urifilters.shorturi
    OLD_CATEGORY_NAMES org.kde.kurifilter-shorturi
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KUriFilter Shorturi (KIO)"
    EXPORT KIO
)

target_link_libraries(kshorturifilter KF6::I18n KF6::KIOWidgets Qt6::DBus)

install( FILES kshorturifilterrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

