set(kcm_webshortcuts_PART_SRCS main.cpp )

add_library(kcm_webshortcuts MODULE ${kcm_webshortcuts_PART_SRCS})

target_link_libraries(kcm_webshortcuts
  PUBLIC
    KF5::ConfigWidgets # KCModule
    KF5::KIOWidgets    # KUriFilter
  PRIVATE
    KF5::I18n)

install(TARGETS kcm_webshortcuts DESTINATION ${KDE_INSTALL_PLUGINDIR})
install(FILES webshortcuts.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
