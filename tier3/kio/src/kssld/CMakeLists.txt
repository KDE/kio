
########### next target ###############

set(kded_kssld_SRCS kssld.cpp )

add_library(kded_kssld MODULE ${kded_kssld_SRCS})

target_link_libraries(kded_kssld
PUBLIC
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons # kdedmodule
PRIVATE
   KF5::Service    # kpluginfactory
   Qt5::Network
   Qt5::DBus
)

install(TARGETS kded_kssld  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############

install( FILES kssld.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )




