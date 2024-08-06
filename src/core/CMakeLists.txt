include (ConfigureChecks.cmake)

configure_file(config-kiocore.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kiocore.h )

configure_file(config-kmountpoint.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmountpoint.h)

configure_file(../kioworkers/file/config-stat-unix.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-stat-unix.h)

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
  krecentdirs.cpp
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
  slavebase.cpp
  workerbase.cpp
  dataworker.cpp
  dataprotocol.cpp
  desktopexecparser.cpp
  emptytrashjob.cpp
  authinfo.cpp
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
  worker.cpp
  workerinterface.cpp
  workerconfig.cpp
  workerfactory.cpp
  workerthread.cpp
  kfilefilter.cpp
  koverlayiconplugin.cpp
  openwith.cpp
  ksslcertificatemanager.cpp
  ksslerroruidata.cpp
)

set(KIOCore_HEADERS_dbus)
if (HAVE_QTDBUS)
  target_sources(KF6KIOCore PRIVATE
    kpasswdserverloop.cpp
    kpasswdserverclient.cpp
    kdirnotify.cpp
    forwardingworkerbase.cpp
  )
  set(KIOCore_HEADERS_dbus KDirNotify)
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

ecm_qt_declare_logging_category(KF6KIOCore
    HEADER kiocoreconnectiondebug.h
    IDENTIFIER KIO_CORE_CONNECTION
    CATEGORY_NAME kf.kio.core.connection
    DEFAULT_SEVERITY Warning
    DESCRIPTION "Connection (KIO)"
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
if (HAVE_QTDBUS)
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KIOFuse.VFS.xml kiofuse_interface)

    set_source_files_properties(org.kde.KPasswdServer.xml
            PROPERTIES INCLUDE authinfo.h
    )
    qt_add_dbus_interface(kiocore_dbus_SRCS org.kde.KPasswdServer.xml kpasswdserver_interface)
endif()

target_sources(KF6KIOCore PRIVATE
    ${kiocore_dbus_SRCS}
)

ecm_generate_export_header(KF6KIOCore
    BASE_NAME KIOCore
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    USE_VERSION_HEADER
    VERSION_BASE_NAME KIO
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 5.101 6.3 6.4 6.6
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF6KIOCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
)

target_include_directories(KF6KIOCore INTERFACE
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore>"
    "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIO>"
)

target_link_libraries(KF6KIOCore
PUBLIC
 KF6::CoreAddons   # KJob
 Qt6::Network
PRIVATE
 Qt6::Xml # davjob.cpp uses QDom
 Qt6::Concurrent
 KF6::ConfigCore
 KF6::I18n
 KF6::Service
)

if (NOT ANDROID)
    target_link_libraries(KF6KIOCore PRIVATE KF6::Crash)
endif()

if (HAVE_QTDBUS)
    target_link_libraries(KF6KIOCore PUBLIC ${DBUS_LIB})
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
  MetaData
  UDSEntry
  JobUiDelegateExtension
  JobUiDelegateFactory
  AskUserActionInterface
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
  AuthInfo
  DavJob
  DesktopExecParser
  FileSystemFreeSpaceJob
  BatchRenameJob
  WorkerBase
  WorkerFactory
  ForwardingWorkerBase
  UntrustedProgramHandlerInterface
  OpenWith

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
  ioworker_defaults.h
  job_base.h
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
  KFileItem
  KFileItemListProperties
  KMountPoint
  KSambaShare
  KSambaShareData
  KPasswdServerClient
  KProtocolInfo
  KProtocolManager
  KRecentDocument
  KRecentDirs
  KSslErrorUiData
  KFileFilter
  KOverlayIconPlugin
  ${KIOCore_HEADERS_dbus}

  REQUIRED_HEADERS KIOCore_HEADERS
)

install(FILES ${KIOCore_CamelCase_HEADERS} DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore/KIO COMPONENT Devel)

install(FILES
  ${KIOCore_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/kiocore_export.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIOCore COMPONENT Devel)

# make available to ecm_add_qch in parent folder
set(KIOCore_QCH_SOURCES ${KIOCore_HEADERS} ${KIO_namespaced_HEADERS} PARENT_SCOPE)

