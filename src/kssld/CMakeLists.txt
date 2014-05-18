
########### next target ###############

set(kded_kssld_SRCS kssld.cpp )

add_library(kded_kssld MODULE ${kded_kssld_SRCS})
set_target_properties(kded_kssld PROPERTIES
    OUTPUT_NAME kssld
)

target_link_libraries(kded_kssld
PUBLIC
   KF5::KIOCore     # ksslcertificatemanager
   KF5::DBusAddons # kdedmodule
PRIVATE
   KF5::Service    # kpluginfactory
   Qt5::Network
   Qt5::DBus
)

install(TARGETS kded_kssld  DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kded )

########### install files ###############

install( FILES kssld.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )




