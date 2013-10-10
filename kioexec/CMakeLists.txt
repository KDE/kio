include_directories(${KDE4_INCLUDE_DIR}/kio ${KDE4_INCLUDE_DIR}/KDE)
set(kioexec_SRCS main.cpp )


kde4_add_executable(kioexec NOGUI ${kioexec_SRCS})

target_link_libraries(kioexec
    KF5::KWidgetsAddons
    ${KDE4Support_LIBRARIES}
    KF5::KI18n
    KF5::KConfigCore
    KF5::KConfigGui
    KF5::KIOCore
    KF5::KIOWidgets
 )

install(TARGETS kioexec DESTINATION ${LIBEXEC_INSTALL_DIR} )

