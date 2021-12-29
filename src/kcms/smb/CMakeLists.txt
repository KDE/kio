add_library(kcm_smb MODULE)

target_sources(kcm_smb PRIVATE
    smbrodlg.cpp
)

target_link_libraries(kcm_smb PRIVATE
    Qt${QT_MAJOR_VERSION}::Widgets
    KF5::CoreAddons
    KF5::ConfigCore
    KF5::I18n
    KF5::ConfigWidgets
)

install(TARGETS kcm_smb DESTINATION ${KDE_INSTALL_PLUGINDIR})

install(FILES smb.desktop DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
