find_package(KF6TextWidgets ${KF_DEP_VERSION} REQUIRED)

add_library(kcm_netpref MODULE)

kcoreaddons_desktop_to_json(kcm_netpref netpref.desktop)
target_sources(kcm_netpref PRIVATE
    netpref.cpp
    ../ksaveioconfig.cpp
)

target_link_libraries(kcm_netpref
  PUBLIC
    Qt${QT_MAJOR_VERSION}::DBus
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::KIOCore
    KF6::TextWidgets # KPluralHandlingSpinBox
  PRIVATE
    KF6::I18n
    KF6::CoreAddons
)

install(TARGETS kcm_netpref  DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.102.0)
  install(FILES netpref.desktop  DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
endif()
