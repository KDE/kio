find_package(Qt6Test REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(KF6Archive ${KF_DEP_VERSION} REQUIRED)
find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "https://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required for httpfiltertest"
                      )

set(kioworker-http_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/kioworkers/http")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/../../src/kioworkers/http" "${kioworker-http_SOURCE_DIR}")

include(ECMAddTests)

ecm_add_test(httpheadertokenizetest.cpp
   NAME_PREFIX "kioworker-"
   LINK_LIBRARIES
   KF6::I18n
   Qt6::Test
   Qt6::Widgets
)

ecm_add_test(httpheaderdispositiontest.cpp
   NAME_PREFIX "kioworker-"
   LINK_LIBRARIES
   KF6::I18n
   Qt6::Test
   Qt6::Widgets
)

ecm_add_test(
   httpauthenticationtest.cpp
   ${kioworker-http_SOURCE_DIR}/kntlm.cpp
   ${kioworker-http_SOURCE_DIR}/des.cpp
   TEST_NAME "httpauthenticationtest"
   NAME_PREFIX "kioworker-"
   LINK_LIBRARIES
   Qt6::Test
   Qt6::Network
   Qt6::Widgets
   Qt6::Core5Compat
   KF6::I18n
   KF6::ConfigCore
   KF6::KIOCore
)

if(GSSAPI_FOUND)
  target_link_libraries(httpauthenticationtest ${GSSAPI_LIBS})
endif()

ecm_add_test(
   httpobjecttest.cpp
   ${kioworker-http_SOURCE_DIR}/http.cpp
   ${kioworker-http_SOURCE_DIR}/httpauthentication.cpp
   ${kioworker-http_SOURCE_DIR}/httpfilter.cpp
   ${kioworker-http_SOURCE_DIR}/kntlm.cpp
   ${kioworker-http_SOURCE_DIR}/des.cpp
   TEST_NAME "httpobjecttest" NAME_PREFIX "kioworker-"
   LINK_LIBRARIES
   Qt6::Test
   Qt6::DBus
   Qt6::Widgets
   Qt6::Network # QLocalSocket
   Qt6::Xml # QDomElement
   KF6::I18n
   KF6::ConfigCore
   KF6::KIOCore
   KF6::Archive
)
if(GSSAPI_FOUND)
  target_link_libraries(httpobjecttest ${GSSAPI_LIBS})
endif()

ecm_add_test(httpfiltertest.cpp ${kioworker-http_SOURCE_DIR}/httpfilter.cpp
             TEST_NAME httpfiltertest
             LINK_LIBRARIES Qt6::Test KF6::I18n KF6::Archive ZLIB::ZLIB)
