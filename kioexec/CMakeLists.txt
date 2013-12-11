include_directories(${KDE4_INCLUDE_DIR}/kio ${KDE4_INCLUDE_DIR}/KDE)
set(kioexec_SRCS main.cpp )


kde4_add_executable(kioexec NOGUI ${kioexec_SRCS})

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

