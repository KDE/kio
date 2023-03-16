kcoreaddons_add_plugin(kcm_smb SOURCES smbrodlg.cpp INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_smb)
target_sources(kcm_smb PRIVATE
    smbrodlg.cpp
)

target_link_libraries(kcm_smb PRIVATE
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::KCMUtils
)
