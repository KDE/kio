add_library(kcm_smb MODULE)

kcoreaddons_desktop_to_json(kcm_smb smb.desktop)
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

install(TARGETS kcm_smb DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.102.0)
    install(FILES smb.desktop DESTINATION  ${KDE_INSTALL_KSERVICESDIR})
endif()
