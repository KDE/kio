if(POLICY CMP0028)
  cmake_policy(SET CMP0028 OLD)
endif()
remove_definitions(-DQT_NO_CAST_FROM_ASCII)

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
 deletejobtest.cpp
 NAME_PREFIX "kiocore-"
 LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
)

target_link_libraries(threadtest Qt5::Concurrent)

if(UNIX)
  ecm_add_tests(
    klocalsockettest.cpp
    klocalsocketservertest.cpp
    NAME_PREFIX "kiocore-"
    LINK_LIBRARIES KF5::KIOCore KF5::I18n Qt5::Test Qt5::Network
  )
endif()

if (TARGET KF5::KIOGui)
  ecm_add_tests(
    favicontest.cpp
    NAME_PREFIX "kiogui-"
    LINK_LIBRARIES KF5::KIOCore KF5::KIOGui Qt5::Test
  )

  target_link_libraries(favicontest Qt5::Concurrent)
endif()

if (TARGET KF5::KIOWidgets)
ecm_add_tests(
 clipboardupdatertest.cpp
 dropjobtest.cpp
 krununittest.cpp
 kdirlistertest.cpp
 kdirmodeltest.cpp
 kfileitemactionstest.cpp
 fileundomanagertest.cpp
 kurifiltertest.cpp
 kurlcompletiontest.cpp
 kurlrequestertest.cpp
 jobguitest.cpp
 pastetest.cpp
 accessmanagertest.cpp
 kurifiltersearchprovideractionstest.cpp
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
)
set_target_properties(krununittest PROPERTIES COMPILE_FLAGS "-DCMAKE_INSTALL_FULL_LIBEXECDIR_KF5=\\\"${CMAKE_INSTALL_FULL_LIBEXECDIR_KF5}\\\"")

# Same as accessmanagertest, but using QNetworkAccessManager, to make sure we
# behave the same
ecm_add_test(
 accessmanagertest.cpp
 TEST_NAME accessmanagertest-qnam
 NAME_PREFIX "kiowidgets-"
 LINK_LIBRARIES KF5::KIOCore KF5::KIOWidgets Qt5::Test
)
set_target_properties(accessmanagertest-qnam PROPERTIES COMPILE_FLAGS "-DUSE_QNAM")

endif()

if (TARGET KF5::KIOFileWidgets)
find_package(KF5XmlGui ${KF5_DEP_VERSION} REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/src/filewidgets ${CMAKE_BINARY_DIR}/src/filewidgets)
ecm_add_tests(
 kurlnavigatortest.cpp
 kurlcomboboxtest.cpp
 kdiroperatortest.cpp
 knewfilemenutest.cpp
 kfilecopytomenutest.cpp
 kfileplacesmodeltest.cpp
 NAME_PREFIX "kiofilewidgets-"
 LINK_LIBRARIES KF5::KIOFileWidgets KF5::KIOWidgets KF5::XmlGui KF5::Bookmarks Qt5::Test
)
endif()

