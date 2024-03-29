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
    CATEGORY_NAME kf.kio.workers.%{APPNAMELC}
    DESCRIPTION "KIO worker (%{APPNAME})"
    DEFAULT_SEVERITY Debug # Change "Debug" to "Info" once doing first releases
    EXPORT %{APPNAMEUC}
)

target_link_libraries(%{APPNAMELC}
    KF6::KIOCore
    KF6::I18n
)

install(TARGETS %{APPNAMELC} DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kio)

ecm_qt_install_logging_categories(
    EXPORT %{APPNAMEUC}
    FILE %{APPNAMELC}.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)
