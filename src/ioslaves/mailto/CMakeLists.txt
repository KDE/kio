
add_executable(kmailservice5 kmailservice.cpp)
ecm_mark_nongui_executable(kmailservice5)

target_link_libraries(kmailservice5
   Qt5::Gui
)

install(TARGETS kmailservice5 ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS kmailservice5.desktop DESTINATION ${KDE_INSTALL_APPDIR})

