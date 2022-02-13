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

add_library(KF5KIOCore)
add_library(KF5::KIOCore ALIAS KF5KIOCore)

set_target_properties(KF5KIOCore PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOCore
)

target_sources(KF5KIOCore PRIVATE
  idleslave.cpp
  connectionbackend.cpp
  connection.cpp
  connectionserver.cpp
  krecentdocument.cpp
  kfileitemlistproperties.cpp
  directorysizejob.cpp
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
  mimetypefinderjob.cpp
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

  kioglobal_p.cpp
  batchrenamejob.cpp
)

if (NOT ANDROID)
target_sources(KF5KIOCore PRIVATE
  tcpslavebase.cpp
  forwardingslavebase.cpp
  kpasswdserverloop.cpp
  kpasswdserverclient.cpp
  kdirnotify.cpp

  ksslcertificatemanager.cpp
  ksslerroruidata.cpp
  ktcpsocket.cpp
  kssl/ksslsettings.cpp
)
endif()

ecm_qt_declare_logging_category(KF5KIOCore
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
   target_sources(KF5KIOCore PRIVATE
      kioglobal_p_unix.cpp
   )
endif()
if (WIN32)
   target_sources(KF5KIOCore PRIVATE
      kioglobal_p_win.cpp
   )
endif()

set(kiocore_dbus_SRCS)
if (TARGET Qt${QT_MAJOR_VERSION}::DBus)
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KIOFuse.VFS.xml kiofuse_interface)

    set_source_files_properties(org.kde.KPasswdServer.xml
            PROPERTIES INCLUDE authinfo.h
    )
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
endif()

target_sources(KF5KIOCore PRIVATE
    ${kiocore_dbus_SRCS}
)

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

ecm_generate_export_header(KF5KIOCore
    BASE_NAME KIOCore
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 3.0 3.1 3.4 4.0 4.3 4.5 4.6 5.0 5.2 5.8 5.24 5.45 5.48 5.63 5.61 5.64 5.65 5.66 5.69 5.72 5.78 5.79 5.80 5.81 5.82 5.83 5.84 5.86 5.88 5.90 5.91
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KIO libs
# needs fixing of undeprecated API being still implemented using own deprecated API

target_include_directories(KF5KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kssl>"
)

target_include_directories(KF5KIOCore INTERFACE
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore>"
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIO>"
)

target_link_libraries(KF5KIOCore
PUBLIC
 KF5::CoreAddons   # KJob
 KF5::Service # TODO KF6 move to PRIVATE
 Qt${QT_MAJOR_VERSION}::Network
PRIVATE
 Qt${QT_MAJOR_VERSION}::Xml # davjob.cpp uses QDom
 KF5::ConfigCore
 KF5::I18n
 KF5::Crash
)
if (TARGET Qt${QT_MAJOR_VERSION}::DBus)
    target_link_libraries(KF5KIOCore
    PUBLIC
        Qt${QT_MAJOR_VERSION}::DBus
    PRIVATE
        KF5::DBusAddons            # KDEInitInterface
    )
endif()

if(KIO_NO_PUBLIC_QTCONCURRENT)
  target_link_libraries(KF5KIOCore PRIVATE Qt${QT_MAJOR_VERSION}::Concurrent) # QtConcurrentRun in hostinfo.cpp
else()
  target_link_libraries(KF5KIOCore PUBLIC Qt${QT_MAJOR_VERSION}::Concurrent) # TODO KF6: remove
endif()

if (TARGET KF5::AuthCore)
  target_link_libraries(KF5KIOCore PRIVATE KF5::AuthCore)   #SlaveBase uses KAuth::Action
endif()

if(ACL_FOUND)
  target_link_libraries(KF5KIOCore PRIVATE ${ACL_LIBS})
endif()

if(HAVE_LIB_MOUNT)
  target_link_libraries(KF5KIOCore PRIVATE LibMount::LibMount)
endif()

if (TARGET Qt6::Core5Compat)
    target_link_libraries(KF5KIOCore PRIVATE Qt6::Core5Compat) # QTextCodec
endif()

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND Qt${QT_MAJOR_VERSION}Core_VERSION VERSION_GREATER 5.8.0)
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
  MimeTypeFinderJob
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
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore/kio COMPONENT Devel
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
install(FILES ${KIOCore_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore/KIO COMPONENT Devel)

install(FILES
  ksslcertificatemanager_p.h
  ${KIOCore_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiocore_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore COMPONENT Devel)

install(FILES accept-languages.codes DESTINATION ${KDE_INSTALL_CONFDIR})

# make available to ecm_add_qch in parent folder
set(KIOCore_QCH_SOURCES ${KIOCore_HEADERS} ${KIO_namespaced_HEADERS} PARENT_SCOPE)

include(ECMGeneratePriFile)
ecm_generate_pri_file(
  BASE_NAME KIOCore
  LIB_NAME KF5KIOCore
  DEPS "KCoreAddons KService"
  FILENAME_VAR PRI_FILENAME
  INCLUDE_INSTALL_DIRS
    ${KDE_INSTALL_INCLUDEDIR_KF}/KIO
    ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore
)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
