add_library(kcm_smb MODULE)

kcoreaddons_desktop_to_json(kcm_smb smb.desktop)
target_sources(kcm_smb PRIVATE
    smbrodlg.cpp
)

target_link_libraries(kcm_smb PRIVATE
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
    KF6::ConfigWidgets
)

install(TARGETS kcm_smb DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings)
