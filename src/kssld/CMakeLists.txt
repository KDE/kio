
########### kssld kiod module ###############

set(kssld_SRCS kssld.cpp )

kcoreaddons_add_plugin(kssld INSTALL_NAMESPACE "kf5/kiod" JSON kssld.json SOURCES ${kssld_SRCS})

target_link_libraries(kssld
PUBLIC
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons # kdedmodule
PRIVATE
   KF5::CoreAddons  # kpluginfactory
   Qt5::Network
   Qt5::DBus
)

kdbusaddons_generate_dbus_service_file(kiod5 org.kde.kssld5 ${KDE_INSTALL_FULL_LIBEXECDIR_KF5})
