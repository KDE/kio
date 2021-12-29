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

install(TARGETS kcm_cookies DESTINATION ${KDE_INSTALL_PLUGINDIR})

install(FILES cookies.desktop DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
