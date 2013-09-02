include_directories(${KDE4_INCLUDE_DIR}/kio ${KDE4_INCLUDE_DIR}/KDE)
set(kioexec_SRCS main.cpp )


kde4_add_executable(kioexec NOGUI ${kioexec_SRCS})

target_link_libraries(kioexec ${KDE4_KIO_LIBS} ${KDE4Support_LIBRARIES}
    ${KDECLARATIVE_LIBRARIES} ${KDE4_SOLID_LIBS}
     KF5::KI18n KF5::KConfigCore KF5::KConfigGui
      ${KCore_LIBRARIES}
 kdeqt5staging
 ${KWindowSystem_LIBRARIES}
 ${KCoreAddons_LIBRARIES}
 plasma
 ${Qt5Script_LIBRARIES}
 
 ${Solid_LIBRARIES}
 ${KIconThemes_LIBRARIES}
 kdeclarative
 )

install(TARGETS kioexec DESTINATION ${LIBEXEC_INSTALL_DIR} )

