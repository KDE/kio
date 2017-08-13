include(ECMMarkNonGuiExecutable)

set(kiod_SRCS kiod_main.cpp)
if (APPLE)
    set(kiod_SRCS ${kiod_SRCS} kiod_agent.mm)
endif()

add_executable(kiod5 ${kiod_SRCS})

target_link_libraries(kiod5
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons  # kdedmodule
   KF5::CoreAddons  # kpluginfactory
   Qt5::Network
   Qt5::DBus
   Qt5::Widgets     # QApplication
)

if (APPLE)
    target_link_libraries(kiod5 "-framework AppKit -framework CoreFoundation")

    # Mark it as non-gui so we won't create an app bundle on Mac OS X
    ecm_mark_nongui_executable(kiod5)
endif ()

install(TARGETS kiod5 DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5})

configure_file(org.kde.kiod5.service.in
   ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kiod5.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kiod5.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

