remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

include(ECMAddTests)

add_subdirectory(http)
add_subdirectory(kcookiejar)

find_package(Qt5Widgets REQUIRED)

########### unittests ###############

find_package(Qt5Concurrent ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

ecm_add_tests(
 kacltest.cpp
 listdirtest.cpp
 kmountpointtest.cpp
 upurltest.cpp
 dataprotocoltest.cpp
 jobtest.cpp
 jobremotetest.cpp
 kfileitemtest.cpp
 kprotocolinfotest.cpp
 ktcpsockettest.cpp
 globaltest.cpp
 mkpathjobtest.cpp
 threadtest.cpp
 udsentrytest.cpp
 udsentry_benchmark.cpp
 kcoredirlister_benchmark.cpp
 deletejobtest.cpp
 urlutiltest.cpp
 batchrenamejobtest.cpp
 ksambasharetest.cpp
 NAME_PREFIX "kiocore-"
 LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

target_link_libraries(threadtest Qt5::Concurrent)

ecm_add_test(
    http_jobtest.cpp
    httpserver_p.cpp
    TEST_NAME http_jobtest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

include(FindGem)
find_gem(ftpd)
set_package_properties(Gem_ftpd PROPERTIES
    DESCRIPTION "Ruby gem 'ftpd' required for testing the ftp slave.")

if(Gem_ftpd_FOUND)
    add_definitions(-DRubyExe_EXECUTABLE="${RubyExe_EXECUTABLE}")
    ecm_add_tests(
        ftptest.cpp
        NAME_PREFIX "kiocore-"
        LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
    )
endif()

if(UNIX)
  ecm_add_tests(
    klocalsockettest.cpp
    klocalsocketservertest.cpp
    privilegejobtest.cpp
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
  )
endif()

if (TARGET KF5::KIOGui)
  ecm_add_tests(
    favicontest.cpp
    applicationlauncherjobtest.cpp
    commandlauncherjobtest.cpp
    NAME_PREFIX "kiogui-"
    LINK_LIBRARIES KF5::KIOCore KF5::KIOGui Qt5::Test
  )

  target_link_libraries(favicontest Qt5::Concurrent)
endif()

if (TARGET KF5::KIOWidgets)
ecm_add_tests(
 clipboardupdatertest.cpp
 dropjobtest.cpp
 kdynamicjobtrackernowidgetstest.cpp
 krununittest.cpp
 kdirlistertest.cpp
 kdirmodeltest.cpp
 kfileitemactionstest.cpp
 fileundomanagertest.cpp
 kurifiltertest.cpp
 kurlcompletiontest.cpp
 jobguitest.cpp
 pastetest.cpp
 accessmanagertest.cpp
 kurifiltersearchprovideractionstest.cpp
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test Qt5::DBus
)
set_target_properties(krununittest PROPERTIES COMPILE_FLAGS "-DCMAKE_INSTALL_FULL_LIBEXECDIR_KF5=\"\\\"${CMAKE_INSTALL_FULL_LIBEXECDIR_KF5}\\\"\"")

# Same as accessmanagertest, but using QNetworkAccessManager, to make sure we
# behave the same
ecm_add_test(
 accessmanagertest.cpp
 TEST_NAME accessmanagertest-qnam
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
)
set_target_properties(accessmanagertest-qnam PROPERTIES COMPILE_FLAGS "-DUSE_QNAM")

# Same as kurlcompletiontest, but with immediate return, and results posted by thread later
ecm_add_test(
 kurlcompletiontest.cpp
 TEST_NAME kurlcompletiontest-nowait
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
 )
set_target_properties(kurlcompletiontest-nowait PROPERTIES COMPILE_FLAGS "-DNO_WAIT")


endif()

if (TARGET KF5::KIOFileWidgets)
find_package(KF5XmlGui ${KF5_DEP_VERSION} REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/src/filewidgets ${CMAKE_BINARY_DIR}/src/filewidgets)
ecm_add_tests(
 kurlnavigatortest.cpp
 kurlcomboboxtest.cpp
 kdiroperatortest.cpp
 kfilewidgettest.cpp
 kfilecustomdialogtest.cpp
 knewfilemenutest.cpp
 kfilecopytomenutest.cpp
 kfileplacesmodeltest.cpp
 kfileplacesviewtest.cpp
 kurlrequestertest.cpp
 NAME_PREFIX "kiofilewidgets-"
 LINK_LIBRARIES KF5::KIOFileWidgets KF5::KIOWidgets KF5::XmlGui KF5::Bookmarks Qt5::Test KF5::I18n
)

# TODO: fix symbol exports for windows -> 'KSambaShare::KSambaShare': inconsistent dll linkage 
if (NOT WIN32)
ecm_add_test(
    ksambashareprivatetest.cpp
    ../src/core/ksambashare.cpp
    ../src/core/kiocoredebug.cpp
    TEST_NAME ksambashareprivatetest
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore Qt5::Test Qt5::Network
)
endif()

set_tests_properties(kiofilewidgets-kfileplacesmodeltest PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(kiofilewidgets-kfileplacesviewtest PROPERTIES RUN_SERIAL TRUE)
endif()

# this should be done by cmake, see bug 371721
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND Qt5Core_VERSION VERSION_GREATER 5.8.0)
    set_property(TARGET jobtest APPEND PROPERTY AUTOMOC_MOC_OPTIONS --include ${CMAKE_BINARY_DIR}/src/core/moc_predefs.h)
endif()
