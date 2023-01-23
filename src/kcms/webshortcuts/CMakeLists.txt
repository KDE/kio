add_library(kcm_webshortcuts MODULE main.cpp)
kcoreaddons_desktop_to_json(kcm_webshortcuts webshortcuts.desktop)

target_link_libraries(kcm_webshortcuts
  PUBLIC
    KF6::ConfigWidgets # KCModule
    KF6::KIOWidgets    # KUriFilter
  PRIVATE
    KF6::I18n
    KF6::CoreAddons
)

install(TARGETS kcm_webshortcuts DESTINATION ${KDE_INSTALL_PLUGINDIR})

if (NOT WIN32)
  add_custom_target(kcm_webshortcuts_link ALL
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../kcm_webshortcuts.so ${CMAKE_CURRENT_BINARY_DIR}/kcm_webshortcuts.so)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kcm_webshortcuts.so DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)
endif()
