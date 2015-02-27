
kde_enable_exceptions()

include(ConfigureChecks.cmake)
find_package(KF5Notifications ${KF5_DEP_VERSION} REQUIRED) #needed for kpac

configure_file(config-kpac.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kpac.h)

########### next target ###############

find_package(Qt5Script ${REQUIRED_QT_VERSION})

if (Qt5Script_FOUND)

    set(kded_proxyscout_PART_SRCS
      proxyscout.cpp
      script.cpp
      downloader.cpp
      discovery.cpp
    )

    add_library(kded_proxyscout MODULE ${kded_proxyscout_PART_SRCS})
    set_target_properties(kded_proxyscout PROPERTIES
        OUTPUT_NAME proxyscout
    )
    kcoreaddons_desktop_to_json(kded_proxyscout proxyscout.desktop)
    target_link_libraries(kded_proxyscout
       Qt5::Network
       Qt5::Script
       ${KPAC_EXTRA_LIBS}
       KF5::KIOCore      # KIO::get
       KF5::DBusAddons  # KDEDModule
       KF5::I18n
       KF5::Notifications # proxyscout.cpp uses KNotifications
    )

    # this needs -lresolv e.g on Slackware, but not on FreeBSD
    if (HAVE_RESOLV_LIBRARY)
      target_link_libraries(kded_proxyscout resolv)
    endif ()

    install(TARGETS kded_proxyscout  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded )
endif()

########### next target ###############

add_executable(kpac_dhcp_helper kpac_dhcp_helper.c)
ecm_mark_nongui_executable(kpac_dhcp_helper)

if (HAVE_NSL_LIBRARY)
  # Assume Solaris
  target_link_libraries(kpac_dhcp_helper nsl)
endif ()

if (HAVE_SOCKET_LIBRARY)
  target_link_libraries(kpac_dhcp_helper socket)
endif ()

install(TARGETS kpac_dhcp_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF5} )

########### install files ###############

install( FILES proxyscout.notifyrc  DESTINATION  ${KDE_INSTALL_KNOTIFY5RCDIR} )
install( FILES proxyscout.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR}/kded )
