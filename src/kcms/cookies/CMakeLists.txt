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
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::ItemViews
    KF6::ConfigWidgets
    KF6::KIOCore
)

install(TARGETS kcm_cookies DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)
