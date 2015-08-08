set(kiod_SRCS kiod_main.cpp)

add_executable(kiod5 ${kiod_SRCS})

target_link_libraries(kiod5
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons  # kdedmodule
   KF5::CoreAddons  # kpluginfactory
   Qt5::Network
   Qt5::DBus
   Qt5::Widgets     # QApplication
)

install(TARGETS kiod5 DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5})

configure_file(org.kde.kiod5.service.in
   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kiod5.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kiod5.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

