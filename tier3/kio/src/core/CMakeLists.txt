project(KIOCore)

include (ConfigureChecks.cmake)

configure_file(config-kiocore.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiocore.h )

configure_file(config-kmountpoint.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmountpoint.h)

find_package(OpenSSL)
set_package_properties(OpenSSL PROPERTIES DESCRIPTION "Support for secure network communications (SSL and TLS)"
                       URL "http://openssl.org"
                       TYPE RECOMMENDED
                       PURPOSE "KDE uses OpenSSL for the bulk of secure communications, including secure web browsing via HTTPS"
                      )
if(OPENSSL_FOUND)
   set(KSSL_HAVE_SSL 1)
   include_directories(${OPENSSL_INCLUDE_DIR})
endif()

set(kiocore_SRCS
  idleslave.cpp
  klocalsocket.cpp
  socketconnectionbackend.cpp
  connection.cpp
  connectionserver.cpp
  krecentdocument.cpp
  kfileitemlistproperties.cpp
  tcpslavebase.cpp
  directorysizejob.cpp
  forwardingslavebase.cpp
  chmodjob.cpp
  kdiskfreespaceinfo.cpp
  usernotificationhandler.cpp
  ksambasharedata.cpp
  ksambashare.cpp
  knfsshare.cpp
  kfileitem.cpp
  davjob.cpp
  deletejob.cpp
  copyjob.cpp
  filejob.cpp
  mkdirjob.cpp
  kpasswdserverloop.cpp
  kpasswdserver.cpp
  kremoteencoding.cpp
  sessiondata.cpp
  slavebase.cpp
  dataslave.cpp
  dataprotocol.cpp
  dataprotocol.cpp
  desktopexecparser.cpp
  authinfo.cpp
  slaveinterface.cpp
  slave.cpp
  job_error.cpp
  job.cpp
  scheduler.cpp
  slaveconfig.cpp
  kprotocolmanager.cpp
  hostinfo.cpp
  kdirnotify.cpp
  kurlauthorized.cpp
  kacl.cpp
  udsentry.cpp
  global.cpp
  metadata.cpp
  kprotocolinfo.cpp
  kprotocolinfofactory.cpp
  jobtracker.cpp
  jobuidelegateextension.cpp
  jobuidelegatefactory.cpp
  kmountpoint.cpp
  kcoredirlister.cpp

  ksslcertificatemanager.cpp
  ktcpsocket.cpp
  kssl/ksslsettings.cpp
)

if (UNIX)
   set(kiocore_SRCS ${kiocore_SRCS}
      klocalsocket_unix.cpp
   )
endif()
if (WIN32)
   set(kiocore_SRCS ${kiocore_SRCS}
      klocalsocket_win.cpp
   )
endif()

qt5_add_dbus_interface(kiocore_SRCS org.kde.KLauncher.xml klauncher_interface)

set_source_files_properties(org.kde.KPasswdServer.xml
        PROPERTIES INCLUDE authinfo.h
)
qt5_add_dbus_interface(kiocore_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
install(FILES
   org.kde.KDirNotify.xml
   org.kde.KPasswdServer.xml
   DESTINATION ${DBUS_INTERFACES_INSTALL_DIR})

add_library(KF5KIOCore ${kiocore_SRCS})
generate_export_header(KF5KIOCore BASE_NAME KIOCore)
add_library(KF5::KIOCore ALIAS KF5KIOCore)

target_include_directories(KF5KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kssl>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>" # kio/global.h
)

target_link_libraries(KF5KIOCore
PUBLIC
 KF5::CoreAddons   # KJob
 KF5::Service
PRIVATE
 Qt5::Network
 Qt5::Concurrent            # QtConcurrentRun in hostinfo.cpp
 Qt5::Xml                   # davjob.cpp uses QDom
 KF5::I18n
 KF5::DBusAddons           # KDBusConnectionPool
)

if(ACL_FOUND)
  target_link_libraries(KF5KIOCore PRIVATE ${ACL_LIBS})
endif()

set_target_properties(KF5KIOCore PROPERTIES VERSION ${KIO_VERSION_STRING}
                                            SOVERSION ${KIO_SOVERSION}
                                            EXPORT_NAME KIOCore
)

install(TARGETS KF5KIOCore EXPORT KF5KIOTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install( FILES
  idleslave.h # installed for klauncher
  connectionserver.h # installed for klauncher
  tcpslavebase.h
  directorysizejob.h
  forwardingslavebase.h
  chmodjob.h
  deletejob.h
  copyjob.h
  filejob.h
  mkdirjob.h
  slavebase.h
  http_slave_defaults.h
  slaveconfig.h
  global.h
  http.h
  ioslave_defaults.h
  hostinfo.h
  jobtracker.h
  metadata.h
  udsentry.h
  jobuidelegateextension.h
  jobuidelegatefactory.h
  slaveinterface.h
  slave.h
  job.h
  scheduler.h
  jobclasses.h
  authinfo.h
  davjob.h
  desktopexecparser.h
  ${CMAKE_CURRENT_BINARY_DIR}/kiocore_export.h

   DESTINATION ${INCLUDE_INSTALL_DIR}/kio COMPONENT Devel)

install(FILES
  kacl.h
  kurlauthorized.h
  kcoredirlister.h
  kremoteencoding.h
  kdiskfreespaceinfo.h
  kdirnotify.h
  kfileitem.h
  kfileitemlistproperties.h
  kmountpoint.h
  knfsshare.h
  ksambashare.h
  ksambasharedata.h
  kprotocolinfo.h
  kprotocolmanager.h
  krecentdocument.h
  ksslcertificatemanager.h
  ksslcertificatemanager_p.h
  ktcpsocket.h
  kssl/ksslsettings.h
  DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)

install(FILES accept-languages.codes DESTINATION ${CONFIG_INSTALL_DIR})
