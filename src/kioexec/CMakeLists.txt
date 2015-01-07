add_executable(kioexec main.cpp)

configure_file(config-kioexec.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioexec.h)

ecm_mark_nongui_executable(kioexec)

target_link_libraries(kioexec
    Qt5::Widgets
    KF5::I18n
    KF5::KIOWidgets    # KRun
    KF5::WidgetsAddons # KMessageBox
    KF5::WindowSystem  # KStartupInfo
 )

if (HAVE_X11)
    target_link_libraries(kioexec Qt5::X11Extras)
endif()

install(TARGETS kioexec DESTINATION ${KF5_LIBEXEC_INSTALL_DIR} )

