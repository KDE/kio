add_definitions(-DTRANSLATION_DOMAIN=\"%{APPNAMELC}\")

set(%{APPNAMELC}_SRCS
    mydatasystem.cpp
    %{APPNAMELC}.cpp
)

ecm_qt_declare_logging_category(%{APPNAMELC}_SRCS
    HEADER %{APPNAMELC}_log.h
    IDENTIFIER %{APPNAMEUC}_LOG
    CATEGORY_NAME kf.kio.slave.%{APPNAMELC}
    DESCRIPTION "KIO slave (%{APPNAME})"
    DEFAULT_SEVERITY Debug # Change "Debug" to "Info" once doing first releases
    EXPORT %{APPNAMEUC}
)

add_library(%{APPNAMELC} MODULE ${%{APPNAMELC}_SRCS})
set_target_properties(%{APPNAMELC} PROPERTIES OUTPUT_NAME "myproto")

target_link_libraries(%{APPNAMELC}
    KF5::KIOCore
    KF5::I18n
)

install(TARGETS %{APPNAMELC} DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)
install(FILES myproto.protocol  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})

ecm_qt_install_logging_categories(
    EXPORT %{APPNAMEUC}
    FILE %{APPNAMELC}.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)
