
add_executable(kmailservice kmailservice.cpp)
ecm_mark_nongui_executable(kmailservice)

target_link_libraries(kmailservice
   Qt5::Gui
)

install(TARGETS kmailservice ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS kmailservice.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

