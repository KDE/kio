
kde_enable_exceptions()

include(ConfigureChecks.cmake)

# needed for kpac notifications about problems
find_package(KF5Notifications ${KF_DEP_VERSION})
set_package_properties(KF5Notifications PROPERTIES
   DESCRIPTION "Notifications library"
   TYPE OPTIONAL
   PURPOSE "Required to have error notification support in kpac")
if (KF5Notifications_FOUND)
  add_definitions(-DHAVE_KF5NOTIFICATIONS)
endif()

configure_file(config-kpac.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kpac.h)

########### next target ###############

find_package(Qt5Qml ${REQUIRED_QT_VERSION})

if (Qt5Qml_FOUND)

    set(kded_proxyscout_PART_SRCS
      proxyscout.cpp
      script.cpp
      downloader.cpp
      discovery.cpp
    )

    kcoreaddons_add_plugin(kded_proxyscout INSTALL_NAMESPACE "kf5/kded" JSON proxyscout.json SOURCES ${kded_proxyscout_PART_SRCS})
    set_target_properties(kded_proxyscout PROPERTIES
        OUTPUT_NAME proxyscout
    )
    target_link_libraries(kded_proxyscout
       Qt5::Network
       Qt5::Qml
       ${KPAC_EXTRA_LIBS}
       KF5::KIOCore      # KIO::get
       KF5::DBusAddons  # KDEDModule
       KF5::I18n
    )
    if (KF5Notifications_FOUND)
        target_link_libraries(kded_proxyscout KF5::Notifications)
    endif()

    # this needs -lresolv e.g on Slackware, but not on FreeBSD
    if (HAVE_RESOLV_LIBRARY)
      target_link_libraries(kded_proxyscout resolv)
    endif ()
endif()

set_package_properties(Qt5Qml PROPERTIES DESCRIPTION "QtQml, one of the Qt5 modules"
   TYPE OPTIONAL
   PURPOSE "Required to build support for auto-configuration of network proxies"
   )

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
