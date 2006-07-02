
# for strange kio/foo.h includes
include_directories( BEFORE ${CMAKE_SOURCE_DIR}/kio )

########### next target ###############

set(kded_kpasswdserver_PART_SRCS kpasswdserver.cpp )

kde4_automoc(${kded_kpasswdserver_PART_SRCS})

kde4_add_plugin(kded_kpasswdserver ${kded_kpasswdserver_PART_SRCS})


target_link_libraries(kded_kpasswdserver  ${KDE4_KDECORE_LIBS} kwalletclient kio )

kde4_install_libtool_file(${PLUGIN_INSTALL_DIR} kded_kpasswdserver )
install_targets(${PLUGIN_INSTALL_DIR} kded_kpasswdserver )


########### install files ###############





