add_definitions(-DTRANSLATION_DOMAIN=\"kcm_webshortcuts\")

set(kcm_webshortcuts_PART_SRCS main.cpp )

add_library(kcm_webshortcuts MODULE ${kcm_webshortcuts_PART_SRCS})

target_link_libraries(kcm_webshortcuts
  PUBLIC
    KF5::ConfigWidgets
    KF5::KIOWidgets
    KF5::Service
  PRIVATE
    KF5::I18n)

install(TARGETS kcm_webshortcuts DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES webshortcuts.desktop DESTINATION ${SERVICES_INSTALL_DIR})
