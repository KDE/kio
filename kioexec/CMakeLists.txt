
# for strange kio/foo.h includes
include_directories( BEFORE ${CMAKE_SOURCE_DIR}/kio )

########### next target ###############

set(kioexec_SRCS main.cpp )

kde4_automoc(kioexec ${kioexec_SRCS})

kde4_add_executable(kioexec NOGUI ${kioexec_SRCS})

target_link_libraries(kioexec  ${KDE4_KDECORE_LIBS} kio )

install(TARGETS kioexec DESTINATION ${BIN_INSTALL_DIR} )

########### install files ###############


