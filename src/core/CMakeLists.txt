project(KIOCore)

include (ConfigureChecks.cmake)

configure_file(config-kiocore.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiocore.h )

configure_file(config-kmountpoint.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmountpoint.h)

# KSSL_HAVE_SSL only used in kssl/ksslsettings.cpp, but currently ifdefed out
#find_package(OpenSSL)
#set_package_properties(OpenSSL PROPERTIES DESCRIPTION "Support for secure network communications (SSL and TLS)"
#                       URL "http://openssl.org"
#                       TYPE RECOMMENDED
#                       PURPOSE "KDE uses OpenSSL for the bulk of secure communications, including secure web browsing via HTTPS"
#                      )
#if(OPENSSL_FOUND)
#   set(KSSL_HAVE_SSL 1)
#   include_directories(${OPENSSL_INCLUDE_DIR})
#endif()

set(kiocore_SRCS
  idleslave.cpp
  connectionbackend.cpp
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
  mkpathjob.cpp
  kpasswdserverloop.cpp
  kpasswdserverclient.cpp
  kremoteencoding.cpp
  sessiondata.cpp
  slavebase.cpp
  dataslave.cpp
  dataprotocol.cpp
  desktopexecparser.cpp
  emptytrashjob.cpp
  authinfo.cpp
  slaveinterface.cpp
  slave.cpp
  job_error.cpp
  job.cpp
  filecopyjob.cpp
  listjob.cpp
  mimetypejob.cpp
  multigetjob.cpp
  restorejob.cpp
  simplejob.cpp
  specialjob.cpp
  statjob.cpp
  namefinderjob.cpp
  storedtransferjob.cpp
  transferjob.cpp
  filesystemfreespacejob.cpp
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
  askuseractioninterface.cpp
  kmountpoint.cpp
  kcoredirlister.cpp
  faviconscache.cpp
  untrustedprogramhandlerinterface.cpp

  ksslcertificatemanager.cpp
  ksslerroruidata.cpp
  ktcpsocket.cpp
  kssl/ksslsettings.cpp

  kioglobal_p.cpp
  batchrenamejob.cpp
)

ecm_qt_declare_logging_category(kiocore_SRCS
    HEADER kiocoredebug.h
    IDENTIFIER KIO_CORE
    CATEGORY_NAME kf.kio.core
    OLD_CATEGORY_NAMES kf5.kio.core
    DESCRIPTION "KIOCore (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_COPYJOB_DEBUG
    CATEGORY_NAME kf.kio.core.copyjob
    OLD_CATEGORY_NAMES kf5.kio.core.copyjob
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KIO::CopyJob (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_CORE_DIRLISTER
    CATEGORY_NAME kf.kio.core.dirlister
    OLD_CATEGORY_NAMES kf5.kio.core.dirlister
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KCoreDirLister (KIO)"
    EXPORT KIO
)

ecm_qt_export_logging_category(
    IDENTIFIER KIO_CORE_SAMBASHARE
    CATEGORY_NAME kf.kio.core.sambashare
    OLD_CATEGORY_NAMES kf5.kio.core.sambashare
    DEFAULT_SEVERITY Warning
    DESCRIPTION "sambashare (KIO)"
    EXPORT KIO
)

if (UNIX)
   set(kiocore_SRCS ${kiocore_SRCS}
      kioglobal_p_unix.cpp
   )
endif()
if (WIN32)
   set(kiocore_SRCS ${kiocore_SRCS}
      kioglobal_p_win.cpp
   )
endif()

qt5_add_dbus_interface(kiocore_SRCS org.kde.KSlaveLauncher.xml klauncher_interface)
qt5_add_dbus_interface(kiocore_SRCS org.kde.KIOFuse.VFS.xml kiofuse_interface)

set_source_files_properties(org.kde.KPasswdServer.xml
        PROPERTIES INCLUDE authinfo.h
)
qt5_add_dbus_interface(kiocore_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
install(FILES
   org.kde.KDirNotify.xml
   DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
   RENAME kf5_org.kde.KDirNotify.xml)
install(FILES
   org.kde.KPasswdServer.xml
   DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
   RENAME kf5_org.kde.KPasswdServer.xml)
install(FILES
   org.kde.KSlaveLauncher.xml
   DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
   RENAME kf5_org.kde.KSlaveLauncher.xml)

add_library(KF5KIOCore ${kiocore_SRCS})
add_library(KF5::KIOCore ALIAS KF5KIOCore)
ecm_generate_export_header(KF5KIOCore
    BASE_NAME KIOCore
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 3.0 3.1 3.4 4.0 4.3 4.5 4.6 5.0 5.2 5.8 5.24 5.45 5.48 5.63 5.61 5.64 5.65 5.66 5.69 5.72 5.78 5.79
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KIO libs
# needs fixing of undeprecated API being still implemented using own deprecated API

target_include_directories(KF5KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kssl>"
)

target_include_directories(KF5KIOCore INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KIOCore>")

target_link_libraries(KF5KIOCore
PUBLIC
 KF5::CoreAddons   # KJob
 KF5::Service
 Qt5::Network
 Qt5::Concurrent            # QtConcurrentRun in hostinfo.cpp
 Qt5::DBus
PRIVATE
 Qt5::Xml                   # davjob.cpp uses QDom
 KF5::I18n
 KF5::Crash
 KF5::DBusAddons            # KDEInitInterface
)

if (UNIX)
  target_link_libraries(KF5KIOCore PRIVATE KF5::AuthCore)   #SlaveBase uses KAuth::Action
endif()

if(ACL_FOUND)
  target_link_libraries(KF5KIOCore PRIVATE ${ACL_LIBS})
endif()

set_target_properties(KF5KIOCore PROPERTIES VERSION ${KIO_VERSION_STRING}
                                            SOVERSION ${KIO_SOVERSION}
                                            EXPORT_NAME KIOCore
)

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND Qt5Core_VERSION VERSION_GREATER 5.8.0)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h
        COMMAND "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_COMPILER_ARG1}" "-dM" "-E" "-c" "${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp" > ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h
    )
    set_property(TARGET KF5KIOCore APPEND PROPERTY AUTOMOC_MOC_OPTIONS --include ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h)
    set_property(TARGET KF5KIOCore APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h)
endif()

# Headers prefixed with KIO/
ecm_generate_headers(KIOCore_CamelCase_HEADERS
  HEADER_NAMES
  IdleSlave
  ConnectionServer
  TCPSlaveBase
  DirectorySizeJob
  ForwardingSlaveBase
  Job #     ### should forward to job_base.h, not job.h...
  JobTracker
  Global
  ChmodJob
  DeleteJob
  CopyJob
  EmptyTrashJob
  FileJob
  MkdirJob
  MkpathJob
  SlaveBase
  SlaveConfig
  HostInfo
  MetaData
  UDSEntry
  JobUiDelegateExtension
  JobUiDelegateFactory
  AskUserActionInterface
  SlaveInterface
  Slave
  FileCopyJob
  ListJob
  MimetypeJob
  MultiGetJob
  RestoreJob
  SimpleJob
  SpecialJob
  StatJob
  NameFinderJob
  StoredTransferJob
  TransferJob
  Scheduler
  AuthInfo
  DavJob
  DesktopExecParser
  FileSystemFreeSpaceJob
  BatchRenameJob

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_HEADERS
)

# Create local forwarding header for kio/job_base.h
set(REGULAR_HEADER_NAME ${CMAKE_CURRENT_BINARY_DIR}/kio/job_base.h)
if (NOT EXISTS ${REGULAR_HEADER_NAME})
   file(WRITE ${REGULAR_HEADER_NAME} "#include \"${CMAKE_CURRENT_SOURCE_DIR}/job_base.h\"\n")
endif()


install(TARGETS KF5KIOCore EXPORT KF5KIOTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

list(APPEND KIO_namespaced_HEADERS
  http_slave_defaults.h
  ioslave_defaults.h
  job_base.h
  jobclasses.h
)
install(FILES
  ${KIO_namespaced_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOCore/kio COMPONENT Devel
)

# Headers not prefixed with KIO/
ecm_generate_headers(KIOCore_HEADERS
  HEADER_NAMES
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
  KPasswdServerClient
  KProtocolInfo
  KProtocolManager
  KRecentDocument
  KSslCertificateManager
  KSslErrorUiData
  KTcpSocket

  REQUIRED_HEADERS KIOCore_HEADERS
)

ecm_generate_headers(KIOCore_HEADERS
  HEADER_NAMES
  KSSLSettings

  RELATIVE kssl
  REQUIRED_HEADERS KIOCore_HEADERS
)
install(FILES ${KIOCore_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOCore/KIO COMPONENT Devel)

install(FILES
  ksslcertificatemanager_p.h
  ${KIOCore_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiocore_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOCore COMPONENT Devel)

install(FILES accept-languages.codes DESTINATION ${KDE_INSTALL_CONFDIR})

# make available to ecm_add_qch in parent folder
set(KIOCore_QCH_SOURCES ${KIOCore_HEADERS} ${KIO_namespaced_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIOCore LIB_NAME KF5KIOCore DEPS "KCoreAddons KService" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIOCore)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
