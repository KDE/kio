set(kshorturifilter_SRCS kshorturifilter.cpp)

kcoreaddons_add_plugin(kshorturifilter INSTALL_NAMESPACE "kf5/urifilters" JSON kshorturifilter.json SERVICETYPES ${CMAKE_SOURCE_DIR}/src/widgets/kurifilterplugin.desktop SOURCES ${kshorturifilter_SRCS})
ecm_generate_qloggingcategories_source(kshorturifilter
    HEADER kshorturifilter_debug.h
    GROUP KSHORTURIFILTER
)

target_link_libraries(kshorturifilter KF5::I18n KF5::KIOWidgets Qt5::DBus)

install( FILES kshorturifilterrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

