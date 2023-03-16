kcoreaddons_add_plugin(kcm_webshortcuts SOURCES main.cpp INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")

kcmutils_generate_desktop_file(kcm_webshortcuts)

target_link_libraries(kcm_webshortcuts
  PUBLIC
    KF6::KCMUtils # KCModule
    KF6::KIOWidgets    # KUriFilter
  PRIVATE
    KF6::I18n
    KF6::CoreAddons
)
