
add_executable(ktelnetservice5 ktelnetservice.cpp)
ecm_mark_nongui_executable(ktelnetservice5)

target_link_libraries(ktelnetservice5
   Qt${QT_MAJOR_VERSION}::Widgets
   KF6::I18n
   KF6::KIOGui
)

install(TARGETS ktelnetservice5 ${KF_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS ktelnetservice5.desktop DESTINATION ${KDE_INSTALL_APPDIR})

