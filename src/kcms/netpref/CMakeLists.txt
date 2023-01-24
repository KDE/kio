find_package(KF6TextWidgets ${KF_DEP_VERSION} REQUIRED)

add_library(kcm_netpref MODULE)

kcoreaddons_desktop_to_json(kcm_netpref netpref.desktop)
target_sources(kcm_netpref PRIVATE
    netpref.cpp
    ../ksaveioconfig.cpp
)

target_link_libraries(kcm_netpref
  PUBLIC
    Qt6::DBus
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::KIOCore
    KF6::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF6::I18n
    KF6::CoreAddons
)

install(TARGETS kcm_netpref  DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)
