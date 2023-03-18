find_package(KF6TextWidgets ${KF_DEP_VERSION} REQUIRED)

kcoreaddons_add_plugin(kcm_netpref INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_netpref)
target_sources(kcm_netpref PRIVATE
    netpref.cpp
    ../ksaveioconfig.cpp
)

target_link_libraries(kcm_netpref
  PUBLIC
    Qt6::DBus
    KF6::ConfigCore
    KF6::KCMUtils
    KF6::KIOCore
    KF6::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF6::I18n
    KF6::CoreAddons
)

if (ENABLE_PCH)
    target_precompile_headers(kcm_netpref REUSE_FROM KIOPchCore)
endif()
