
########### kssld kiod module ###############

kcoreaddons_add_plugin(kssld
    INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/kiod"
)

target_sources(kssld PRIVATE
    kssld.cpp
)

target_link_libraries(kssld
PUBLIC
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons # kdedmodule
PRIVATE
   KF5::CoreAddons  # kpluginfactory
   KF5::ConfigCore
   Qt${QT_MAJOR_VERSION}::Network
   Qt${QT_MAJOR_VERSION}::DBus
)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kssld5 ${KDE_INSTALL_FULL_LIBEXECDIR_KF})
