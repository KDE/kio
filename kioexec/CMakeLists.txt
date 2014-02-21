add_executable(kioexec main.cpp)

ecm_mark_nongui_executable(kioexec)

target_link_libraries(kioexec
    KF5::WidgetsAddons
    KF5::KDE4Support
    KF5::I18n
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::KIOCore
    KF5::KIOWidgets
 )

install(TARGETS kioexec DESTINATION ${LIBEXEC_INSTALL_DIR} )

