include(ECMMarkNonGuiExecutable)

# procotol to json converter
add_executable(protocoltojson main.cpp)

# Mark it as non-gui so we won't create an app bundle on Mac OS X
ecm_mark_nongui_executable(protocoltojson)

target_link_libraries(protocoltojson KF5::ConfigCore Qt${QT_MAJOR_VERSION}::Core)

install(TARGETS protocoltojson ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
