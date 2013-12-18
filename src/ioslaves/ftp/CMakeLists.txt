
project(kioslave-ftp)

remove_definitions(-DQT_NO_CAST_FROM_ASCII) # TODO REMOVE

include(ConfigureChecks.cmake)
configure_file(config-kioslave-ftp.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-ftp.h )

########### next target ###############

set(kio_ftp_PART_SRCS
ftp.cpp
)


add_library(kio_ftp MODULE ${kio_ftp_PART_SRCS})
set_target_properties(kio_file PROPERTIES PREFIX "")
target_link_libraries(kio_ftp Qt5::Network KF5::KIOCore KF5::I18n)

install(TARGETS kio_ftp  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES ftp.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )




