
########### kssld kiod module ###############

set(kssld_SRCS kssld.cpp )

add_library(kssld MODULE ${kssld_SRCS})

target_link_libraries(kssld
PUBLIC
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons # kdedmodule
PRIVATE
   KF5::CoreAddons  # kpluginfactory
   Qt5::Network
   Qt5::DBus
)

kcoreaddons_desktop_to_json(kssld kssld.desktop)

install(TARGETS kssld  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kiod )

