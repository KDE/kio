add_definitions(-DTRANSLATION_DOMAIN=\"%{APPNAMELC}\")

add_library(%{APPNAMELC} MODULE)
set_target_properties(%{APPNAMELC} PROPERTIES OUTPUT_NAME "myproto")

target_sources(%{APPNAMELC} PRIVATE
    mydatasystem.cpp
    %{APPNAMELC}.cpp
)

ecm_qt_declare_logging_category(%{APPNAMELC}
    HEADER %{APPNAMELC}_log.h
    IDENTIFIER %{APPNAMEUC}_LOG
    CATEGORY_NAME kf.kio.slave.%{APPNAMELC}
    DESCRIPTION "KIO slave (%{APPNAME})"
    DEFAULT_SEVERITY Debug # Change "Debug" to "Info" once doing first releases
    EXPORT %{APPNAMEUC}
)

target_link_libraries(%{APPNAMELC}
    KF5::KIOCore
    KF5::I18n
)

install(TARGETS %{APPNAMELC} DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf${QT_MAJOR_VERSION}/kio)
install(FILES myproto.protocol  DESTINATION  ${KDE_INSTALL_KSERVICESDIR})

ecm_qt_install_logging_categories(
    EXPORT %{APPNAMEUC}
    FILE %{APPNAMELC}.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)
