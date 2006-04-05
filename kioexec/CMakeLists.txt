kde4_header()

# for strange kio/foo.h includes
include_directories( BEFORE ${CMAKE_SOURCE_DIR}/kio )

########### next target ###############

set(kioexec_SRCS main.cpp )

kde4_automoc(${kioexec_SRCS})

kde4_add_executable(kioexec NOGUI ${kioexec_SRCS})

target_link_libraries(kioexec  ${KDE4_KDECORE_LIBS} kio )

install_targets(/bin kioexec )

########### install files ###############

kde4_footer()

