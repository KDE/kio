
add_executable(ktelnetservice ktelnetservice.cpp)
ecm_mark_nongui_executable(ktelnetservice)

target_link_libraries(ktelnetservice
   KF5::Service
   KF5::WidgetsAddons
   KF5::I18n
   Qt5::Widgets
)

install(TARGETS ktelnetservice ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS ktelnetservice.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

