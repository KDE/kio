remove_definitions(-DQT_NO_CAST_FROM_ASCII)

include(ECMAddTests)

add_subdirectory(http)
add_subdirectory(kcookiejar)

find_package(Qt5Widgets REQUIRED)

########### unittests ###############

find_package(Qt5Concurrent 5.2.0 REQUIRED NO_MODULE)

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
 udsentrytest.cpp
 NAME_PREFIX "kiocore-"
 LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

if(UNIX)
  ecm_add_tests(
    klocalsockettest.cpp
    klocalsocketservertest.cpp
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
  )
endif()

ecm_add_tests(
 clipboardupdatertest.cpp
 krununittest.cpp
 kdirlistertest.cpp
 kdirmodeltest.cpp
 kfileitemactionstest.cpp
 fileundomanagertest.cpp
 kurlcompletiontest.cpp
 kurlrequestertest.cpp
 jobguitest.cpp
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
)

set_target_properties(kiowidgets-krununittest PROPERTIES COMPILE_FLAGS "-DCMAKE_INSTALL_PREFIX=\\\"${CMAKE_INSTALL_PREFIX}\\\" -DKF5_LIBEXEC_INSTALL_DIR=\\\"${KF5_LIBEXEC_INSTALL_DIR}\\\"")

include_directories(${CMAKE_SOURCE_DIR}/src/filewidgets ${CMAKE_BINARY_DIR}/src/filewidgets)
ecm_add_tests(
 kurlnavigatortest.cpp
 kdiroperatortest.cpp
 knewfilemenutest.cpp
 kfileplacesmodeltest.cpp
 NAME_PREFIX "kiofilewidgets-"
 LINK_LIBRARIES KF5::KIOFileWidgets KF5::KIOWidgets Qt5::Test
)

