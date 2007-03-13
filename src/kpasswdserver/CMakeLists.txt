set(kded_kpasswdserver_SRCS kpasswdserver.cpp )

kde4_automoc(${kded_kpasswdserver_SRCS})

kde4_add_plugin(kded_kpasswdserver ${kded_kpasswdserver_SRCS})

target_link_libraries(kded_kpasswdserver  ${KDE4_KIO_LIBS} ${KDE4_KWALLETCLIENT_LIBS} )

install(TARGETS kded_kpasswdserver  DESTINATION ${PLUGIN_INSTALL_DIR} )

