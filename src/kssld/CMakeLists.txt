
########### kssld kiod module ###############

kcoreaddons_add_plugin(kssld
    INSTALL_NAMESPACE "kf6/kiod"
)

target_sources(kssld PRIVATE
    kssld.cpp
)

target_link_libraries(kssld
PUBLIC
   KF6::KIOCore     # ksslcertificatemanager
   KF6::DBusAddons # kdedmodule
PRIVATE
   KF6::CoreAddons  # kpluginfactory
   KF6::ConfigCore
   Qt6::Network
   ${DBUS_LIB}
)

kdbusaddons_generate_dbus_service_file(kiod6 org.kde.kssld6 ${KDE_INSTALL_FULL_LIBEXECDIR_KF})
