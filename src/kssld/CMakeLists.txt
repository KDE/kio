
########### kssld kiod module ###############

kcoreaddons_add_plugin(kssld
    INSTALL_NAMESPACE "kf5/kiod"
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
   Qt5::Network
   Qt5::DBus
)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kssld5 ${KDE_INSTALL_FULL_LIBEXECDIR_KF5})
