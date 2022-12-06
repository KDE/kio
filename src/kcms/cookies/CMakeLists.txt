add_library(kcm_cookies MODULE)

kcoreaddons_desktop_to_json(kcm_cookies cookies.desktop)

target_sources(kcm_cookies PRIVATE
    kcookiesmain.cpp
    kcookiespolicies.cpp
    kcookiesmanagement.cpp
    kcookiespolicyselectiondlg.cpp
    ../ksaveioconfig.cpp
)

ki18n_wrap_ui(kcm_cookies
    kcookiespolicies.ui
    kcookiesmanagement.ui
    kcookiespolicyselectiondlg.ui
)

target_link_libraries(kcm_cookies PRIVATE
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::DBus
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::I18n
    KF5::ItemViews
    KF5::ConfigWidgets
    KF5::KIOCore
)

install(TARGETS kcm_cookies DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.102.0)
    install(FILES cookies.desktop DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
endif()
