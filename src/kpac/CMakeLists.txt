
kde_enable_exceptions()

include(ConfigureChecks.cmake)

# needed for kpac notifications about problems
find_package(KF6Notifications ${KF_DEP_VERSION})
set_package_properties(KF6Notifications PROPERTIES
   DESCRIPTION "Notifications library"
   TYPE OPTIONAL
   PURPOSE "Required to have error notification support in kpac")
if (KF6Notifications_FOUND)
  add_definitions(-DHAVE_KF6NOTIFICATIONS)
endif()

configure_file(config-kpac.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kpac.h)

########### next target ###############

find_package(Qt6Qml ${REQUIRED_QT_VERSION})

if (TARGET Qt6::Qml)
    kcoreaddons_add_plugin(kded_proxyscout
        INSTALL_NAMESPACE "kf6/kded"
    )
    set_target_properties(kded_proxyscout PROPERTIES
        OUTPUT_NAME proxyscout
    )

    target_sources(kded_proxyscout PRIVATE
      proxyscout.cpp
      script.cpp
      downloader.cpp
      discovery.cpp
    )

    target_link_libraries(kded_proxyscout
    PRIVATE
       Qt6::Network
       Qt6::Qml
       ${KPAC_EXTRA_LIBS}
       KF6::KIOCore      # KIO::get
       KF6::DBusAddons  # KDEDModule
       KF6::I18n
    )
    if (KF6Notifications_FOUND)
        target_link_libraries(kded_proxyscout PRIVATE KF6::Notifications)
    endif()

    # this needs -lresolv e.g on Slackware, but not on FreeBSD
    if (HAVE_RESOLV_LIBRARY)
      target_link_libraries(kded_proxyscout PRIVATE resolv)
    endif ()

    if (TARGET Qt6::Core5Compat)
        target_link_libraries(kded_proxyscout PRIVATE Qt6::Core5Compat) # QTextCodec
    endif()
endif()

set_package_properties(Qt6Qml PROPERTIES DESCRIPTION "QtQml, one of the Qt6 modules"
   TYPE OPTIONAL
   PURPOSE "Required to build support for auto-configuration of network proxies"
   )

ecm_qt_declare_logging_category(kded_proxyscout
    HEADER kio_kpac_debug.h
    IDENTIFIER KIO_KPAC
    CATEGORY_NAME kf.kio.kpac
    DESCRIPTION "KPAC ProxyScout (KIO)"
    EXPORT KIO
)
########### next target ###############

add_executable(kpac_dhcp_helper)
ecm_mark_nongui_executable(kpac_dhcp_helper)

target_sources(kpac_dhcp_helper PRIVATE
    kpac_dhcp_helper.c
)

if (HAVE_NSL_LIBRARY)
  # Assume Solaris
  target_link_libraries(kpac_dhcp_helper nsl)
endif ()

if (HAVE_SOCKET_LIBRARY)
  target_link_libraries(kpac_dhcp_helper socket)
endif ()

install(TARGETS kpac_dhcp_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR_KF} )

########### install files ###############

install( FILES proxyscout.notifyrc  DESTINATION  ${KDE_INSTALL_KNOTIFYRCDIR} )
