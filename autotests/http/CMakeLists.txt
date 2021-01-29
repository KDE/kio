find_package(Qt5Test REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(KF5Archive ${KF_DEP_VERSION} REQUIRED)
find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "http://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required for httpfiltertest"
                      )

include_directories("${CMAKE_CURRENT_BINARY_DIR}/../../src/ioslaves/http" "${CMAKE_CURRENT_SOURCE_DIR}/../../src/ioslaves/http" )

include(ECMAddTests)

ecm_add_test(httpheadertokenizetest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES KF5::I18n Qt5::Test Qt5::Widgets
)

ecm_add_test(httpheaderdispositiontest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES KF5::I18n Qt5::Test Qt5::Widgets
)

ecm_add_test(httpauthenticationtest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES
   Qt5::Test
   Qt5::Network
   Qt5::Widgets
   KF5::I18n
   KF5::KIOCore
   KF5::KIONTLM
)

if(GSSAPI_FOUND)
  target_link_libraries(httpauthenticationtest ${GSSAPI_LIBS})
endif()

set(httpobjecttest_SRCS
   httpobjecttest.cpp
   ${kioslave-http_SOURCE_DIR}/http.cpp
   ${kioslave-http_SOURCE_DIR}/httpauthentication.cpp
   ${kioslave-http_SOURCE_DIR}/httpfilter.cpp
)

ecm_add_test(${httpobjecttest_SRCS}
   TEST_NAME "httpobjecttest" NAME_PREFIX "kioslave-"
   LINK_LIBRARIES
   Qt5::Test
   Qt5::DBus
   Qt5::Widgets
   Qt5::Network # QLocalSocket
   Qt5::Xml # QDomElement
   KF5::I18n
   KF5::KIOCore
   KF5::KIONTLM
   KF5::Archive
)
if(GSSAPI_FOUND)
  target_link_libraries(httpobjecttest ${GSSAPI_LIBS})
endif()

ecm_add_test(httpfiltertest.cpp ${kioslave-http_SOURCE_DIR}/httpfilter.cpp
             TEST_NAME httpfiltertest
             LINK_LIBRARIES Qt5::Test KF5::I18n KF5::Archive ${ZLIB_LIBRARY})
target_include_directories(httpfiltertest PRIVATE ${ZLIB_INCLUDE_DIRS})

