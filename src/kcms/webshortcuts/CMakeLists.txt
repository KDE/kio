add_library(kcm_webshortcuts MODULE main.cpp)
kcoreaddons_desktop_to_json(kcm_webshortcuts webshortcuts.desktop)

target_link_libraries(kcm_webshortcuts
  PUBLIC
    KF5::ConfigWidgets # KCModule
    KF5::KIOWidgets    # KUriFilter
  PRIVATE
    KF5::I18n
    KF5::CoreAddons
)

install(TARGETS kcm_webshortcuts DESTINATION ${KDE_INSTALL_PLUGINDIR})
install(FILES webshortcuts.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
