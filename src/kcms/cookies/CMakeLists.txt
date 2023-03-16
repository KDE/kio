kcoreaddons_add_plugin(kcm_cookies INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_cookies)

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
    Qt6::Widgets
    Qt6::DBus
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::ItemViews
    KF6::KCMUtils
    KF6::KIOCore
)
