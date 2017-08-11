
project(kioslave-ftp)

remove_definitions(-DQT_NO_CAST_FROM_ASCII) # TODO REMOVE

include(ConfigureChecks.cmake)
configure_file(config-kioslave-ftp.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kioslave-ftp.h )

########### next target ###############

set(kio_ftp_PART_SRCS
ftp.cpp
)


add_library(kio_ftp MODULE ${kio_ftp_PART_SRCS})
target_link_libraries(kio_ftp Qt5::Network KF5::KIOCore KF5::I18n)

set_target_properties(kio_ftp PROPERTIES OUTPUT_NAME "ftp")
set_target_properties(kio_ftp PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio")
install(TARGETS kio_ftp  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)
