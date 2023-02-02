
add_executable(ktelnetservice6 ktelnetservice.cpp)
ecm_mark_nongui_executable(ktelnetservice6)

target_link_libraries(ktelnetservice6
   Qt6::Widgets
   KF6::I18n
   KF6::KIOGui
)

install(TARGETS ktelnetservice6 ${KF_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS ktelnetservice6.desktop DESTINATION ${KDE_INSTALL_APPDIR})

