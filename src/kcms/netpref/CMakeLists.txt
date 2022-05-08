find_package(KF5TextWidgets ${KF_DEP_VERSION} REQUIRED)

add_library(kcm_netpref MODULE)

kcoreaddons_desktop_to_json(kcm_netpref netpref.desktop)
target_sources(kcm_netpref PRIVATE
    netpref.cpp
    ../ksaveioconfig.cpp
)

target_link_libraries(kcm_netpref
  PUBLIC
    Qt${QT_MAJOR_VERSION}::DBus
    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::KIOCore
    KF5::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF5::I18n
    KF5::CoreAddons
)

install(TARGETS kcm_netpref  DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)

install(FILES netpref.desktop  DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
