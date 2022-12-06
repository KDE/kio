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

if (NOT WIN32)
  add_custom_target(kcm_webshortcuts_link ALL
                    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../kcm_webshortcuts.so ${CMAKE_CURRENT_BINARY_DIR}/kcm_webshortcuts.so)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kcm_webshortcuts.so DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/kcms/systemsettings_qwidgets)
endif()

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.102.0)
  install(FILES webshortcuts.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
endif()
