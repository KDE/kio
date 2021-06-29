kcoreaddons_add_plugin(kshorturifilter
    INSTALL_NAMESPACE "kf5/urifilters"
    SERVICETYPES ${CMAKE_SOURCE_DIR}/src/widgets/kurifilterplugin.desktop
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

target_link_libraries(kshorturifilter KF5::I18n KF5::KIOWidgets Qt5::DBus)

install( FILES kshorturifilterrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

