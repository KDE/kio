set(kded_kpasswdserver_SRCS kpasswdserver.cpp )

kde4_add_plugin(kded_kpasswdserver ${kded_kpasswdserver_SRCS})

target_link_libraries(kded_kpasswdserver  ${KDE4_KIO_LIBS} ${QT_QT3SUPPORT_LIBRARY})

install(TARGETS kded_kpasswdserver  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kpasswdserver.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
