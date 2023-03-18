kcoreaddons_add_plugin(kcm_proxy INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_smb)

target_sources(kcm_proxy PRIVATE
    kproxydlg.cpp
    ../ksaveioconfig.cpp
)

ki18n_wrap_ui(kcm_proxy
    kproxydlg.ui
)

target_link_libraries(kcm_proxy PRIVATE
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::KCMUtils
    KF6::KIOWidgets
)

if (ENABLE_PCH)
    target_precompile_headers(kcm_proxy REUSE_FROM KIOPchWidgets)
endif()
