include (ConfigureChecks.cmake)

configure_file(config-kiocore.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiocore.h )

configure_file(config-kmountpoint.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmountpoint.h)

# KSSL_HAVE_SSL only used in kssl/ksslsettings.cpp, but currently ifdefed out
#find_package(OpenSSL)
#set_package_properties(OpenSSL PROPERTIES DESCRIPTION "Support for secure network communications (SSL and TLS)"
#                       URL "https://www.openssl.org/"
#                       TYPE RECOMMENDED
#                       PURPOSE "KDE uses OpenSSL for the bulk of secure communications, including secure web browsing via HTTPS"
#                      )
#if(OPENSSL_FOUND)
#   set(KSSL_HAVE_SSL 1)
#   include_directories(${OPENSSL_INCLUDE_DIR})
#endif()

add_library(KF6KIOCore)
add_library(KF6::KIOCore ALIAS KF6KIOCore)

set_target_properties(KF6KIOCore PROPERTIES
    VERSION     ${KIO_VERSION}
    SOVERSION   ${KIO_SOVERSION}
    EXPORT_NAME KIOCore
)

target_sources(KF6KIOCore PRIVATE
  connectionbackend.cpp
  connection.cpp
  connectionserver.cpp
  krecentdocument.cpp
  kfileitemlistproperties.cpp
  directorysizejob.cpp
  chmodjob.cpp
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
  workerbase.cpp
  dataworker.cpp
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
  restorejob.cpp
  simplejob.cpp
  specialjob.cpp
  statjob.cpp
  namefinderjob.cpp
  storedtransferjob.cpp
  transferjob.cpp
  filesystemfreespacejob.cpp
  scheduler.cpp
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
  workerconfig.cpp
  workerfactory.cpp
  workerthread.cpp
  kfilefilter.cpp
)

if (NOT ANDROID)
target_sources(KF6KIOCore PRIVATE
  tcpworkerbase.cpp
  kpasswdserverloop.cpp
  kpasswdserverclient.cpp
  kdirnotify.cpp
  forwardingworkerbase.cpp

  ksslcertificatemanager.cpp
  ksslerroruidata.cpp
  kssl/ksslsettings.cpp
)
endif()

ecm_qt_declare_logging_category(KF6KIOCore
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
   target_sources(KF6KIOCore PRIVATE
      kioglobal_p_unix.cpp
   )
endif()
if (WIN32)
   target_sources(KF6KIOCore PRIVATE
      kioglobal_p_win.cpp
   )
endif()

set(kiocore_dbus_SRCS)
if (TARGET Qt6::DBus)
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KIOFuse.VFS.xml kiofuse_interface)

    set_source_files_properties(org.kde.KPasswdServer.xml
            PROPERTIES INCLUDE authinfo.h
    )
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
endif()

target_sources(KF6KIOCore PRIVATE
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

ecm_generate_export_header(KF6KIOCore
    BASE_NAME KIOCore
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.100 5.101 5.102 5.103
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)
# TODO: add support for EXCLUDE_DEPRECATED_BEFORE_AND_AT to all KIO libs
# needs fixing of undeprecated API being still implemented using own deprecated API

target_include_directories(KF6KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/kssl>"
)

target_include_directories(KF6KIOCore INTERFACE
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore>"
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIO>"
)

target_link_libraries(KF6KIOCore
PUBLIC
 KF6::CoreAddons   # KJob
 KF6::Service # TODO KF6 move to PRIVATE
 Qt6::Network
PRIVATE
 Qt6::Xml # davjob.cpp uses QDom
 Qt6::Concurrent
 KF6::ConfigCore
 KF6::I18n
)

if (NOT ANDROID)
    target_link_libraries(KF6KIOCore PRIVATE KF6::Crash)
endif()

if (TARGET Qt6::DBus)
    target_link_libraries(KF6KIOCore
    PUBLIC
        Qt6::DBus
    )
endif()

if (TARGET KF6::AuthCore)
  target_link_libraries(KF6KIOCore PRIVATE KF6::AuthCore)   #SlaveBase uses KAuth::Action
endif()

if(ACL_FOUND)
  target_link_libraries(KF6KIOCore PRIVATE ${ACL_LIBS})
endif()

if(HAVE_LIB_MOUNT)
  target_link_libraries(KF6KIOCore PRIVATE LibMount::LibMount)
endif()

if (TARGET Qt6::Core5Compat)
    target_link_libraries(KF6KIOCore PRIVATE Qt6::Core5Compat) # QTextCodec
endif()

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h
        COMMAND "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_COMPILER_ARG1}" "-dM" "-E" "-c" "${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp" > ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h
    )
    set_property(TARGET KF6KIOCore APPEND PROPERTY AUTOMOC_MOC_OPTIONS --include ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h)
    set_property(TARGET KF6KIOCore APPEND PROPERTY AUTOGEN_TARGET_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/moc_predefs.h)
endif()

# Headers prefixed with KIO/
ecm_generate_headers(KIOCore_CamelCase_HEADERS
  HEADER_NAMES
  ConnectionServer
  TCPWorkerBase
  DirectorySizeJob
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
  WorkerBase
  WorkerFactory
  ForwardingWorkerBase

  PREFIX KIO
  REQUIRED_HEADERS KIO_namespaced_HEADERS
)

# Create local forwarding header for kio/job_base.h
set(REGULAR_HEADER_NAME ${CMAKE_CURRENT_BINARY_DIR}/kio/job_base.h)
if (NOT EXISTS ${REGULAR_HEADER_NAME})
   file(WRITE ${REGULAR_HEADER_NAME} "#include \"${CMAKE_CURRENT_SOURCE_DIR}/job_base.h\"\n")
endif()


install(TARGETS KF6KIOCore EXPORT KF6KIOTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

list(APPEND KIO_namespaced_HEADERS
  http_worker_defaults.h
  ioworker_defaults.h
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
  KFileFilter

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
  LIB_NAME KF6KIOCore
  DEPS "KCoreAddons KService"
  FILENAME_VAR PRI_FILENAME
  INCLUDE_INSTALL_DIRS
    ${KDE_INSTALL_INCLUDEDIR_KF}/KIO
    ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore
)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
