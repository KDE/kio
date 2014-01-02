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
  filecopyjob.cpp
  listjob.cpp
  mimetypejob.cpp
  multigetjob.cpp
  simplejob.cpp
  specialjob.cpp
  statjob.cpp
  storedtransferjob.cpp
  transferjob.cpp
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

qt5_add_dbus_interface(kiocore_SRCS org.kde.KSlaveLauncher.xml klauncher_interface)

set_source_files_properties(org.kde.KPasswdServer.xml
        PROPERTIES INCLUDE authinfo.h
)
qt5_add_dbus_interface(kiocore_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
install(FILES
   org.kde.KDirNotify.xml
   org.kde.KPasswdServer.xml
   org.kde.KSlaveLauncher.xml
   DESTINATION ${DBUS_INTERFACES_INSTALL_DIR})

add_library(KF5KIOCore ${kiocore_SRCS})
generate_export_header(KF5KIOCore BASE_NAME KIOCore)
add_library(KF5::KIOCore ALIAS KF5KIOCore)

target_include_directories(KF5KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kssl>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>" # kio/global.h
)

target_include_directories(KF5KIOCore INTERFACE "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/KIOCore>" "$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/kiocore>")

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

ecm_generate_headers(
  IdleSlave
  ConnectionServer
  TCPSlaveBase
  DirectorySizeJob
  ForwardingSlaveBase
  Job #     ### should forward to job_base.h, not job.h...
  ChmodJob
  DeleteJob
  CopyJob
  FileJob
  MkdirJob
  SlaveBase
  SlaveConfig
  HostInfo
  MetaData
  UDSEntry
  JobUiDelegateExtension
  JobUiDelegateFactory
  SlaveInterface
  Slave
  FileCopyJob
  ListJob
  MimetypeJob
  MultiGetJob
  SimpleJob
  SpecialJob
  StatJob
  StoredTransferJob
  TransferJob
  Scheduler
  AuthInfo
  DavJob
  DesktopExecParser

  MODULE_NAME KIOCore/KIO
  REQUIRED_HEADERS KIO_namespaced_HEADERS
)

install(TARGETS KF5KIOCore EXPORT KF5KIOTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  http_slave_defaults.h
  global.h
  http.h
  ioslave_defaults.h
  jobtracker.h
  job_base.h
  jobclasses.h
  ${CMAKE_CURRENT_BINARY_DIR}/kiocore_export.h

  ${KIO_namespaced_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/kiocore/kio COMPONENT Devel
)

ecm_generate_headers(
  KACL
  KUrlAuthorized
  KCoreDirLister
  KRemoteEncoding
  KDirNotify
  KDiskFreeSpaceInfo
  KFileItem
  KFileItemListProperties
  KMountPoint
  KNFSShare
  KSambaShare
  KSambaShareData
  KProtocolInfo
  KProtocolManager
  KRecentDocument
  KSslCertificateManager
  KTcpSocket

  MODULE_NAME KIOCore
  REQUIRED_HEADERS KIOCore_HEADERS
)

ecm_generate_headers(
  KSSLSettings

  RELATIVE kssl
  MODULE_NAME KIOCore
  REQUIRED_HEADERS KIOCore_HEADERS
)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KIOCore DESTINATION ${INCLUDE_INSTALL_DIR} COMPONENT Devel)

install(FILES
  ksslcertificatemanager_p.h
  ${KIOCore_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/kiocore COMPONENT Devel)

install(FILES accept-languages.codes DESTINATION ${CONFIG_INSTALL_DIR})
