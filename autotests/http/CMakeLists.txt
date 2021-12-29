find_package(Qt${QT_MAJOR_VERSION}Test REQUIRED)
find_package(Qt${QT_MAJOR_VERSION}Widgets REQUIRED)
find_package(KF5Archive ${KF_DEP_VERSION} REQUIRED)
find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "http://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required for httpfiltertest"
                      )

set(kioslave-http_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/ioslaves/http")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/../../src/ioslaves/http" "${kioslave-http_SOURCE_DIR}")

include(ECMAddTests)

if (TARGET Qt6::Core5Compat)
    set(_qt5_compat_libs Qt6::Core5Compat) # QTextCodec
endif()

ecm_add_test(httpheadertokenizetest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES KF5::I18n Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Widgets ${_qt5_compat_libs}
)

ecm_add_test(httpheaderdispositiontest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES KF5::I18n Qt${QT_MAJOR_VERSION}::Test Qt${QT_MAJOR_VERSION}::Widgets ${_qt5_compat_libs}
)

ecm_add_test(httpauthenticationtest.cpp
   NAME_PREFIX "kioslave-"
   LINK_LIBRARIES
   Qt${QT_MAJOR_VERSION}::Test
   Qt${QT_MAJOR_VERSION}::Network
   Qt${QT_MAJOR_VERSION}::Widgets
   ${_qt5_compat_libs}
   KF5::I18n
   KF5::ConfigCore
   KF5::KIOCore
   KF5::KIONTLM
)

if(GSSAPI_FOUND)
  target_link_libraries(httpauthenticationtest ${GSSAPI_LIBS})
endif()

ecm_add_test(
   httpobjecttest.cpp
   ${kioslave-http_SOURCE_DIR}/http.cpp
   ${kioslave-http_SOURCE_DIR}/httpauthentication.cpp
   ${kioslave-http_SOURCE_DIR}/httpfilter.cpp
   TEST_NAME "httpobjecttest" NAME_PREFIX "kioslave-"
   LINK_LIBRARIES
   Qt${QT_MAJOR_VERSION}::Test
   Qt${QT_MAJOR_VERSION}::DBus
   Qt${QT_MAJOR_VERSION}::Widgets
   Qt${QT_MAJOR_VERSION}::Network # QLocalSocket
   Qt${QT_MAJOR_VERSION}::Xml # QDomElement
   ${_qt5_compat_libs}
   KF5::I18n
   KF5::ConfigCore
   KF5::KIOCore
   KF5::KIONTLM
   KF5::Archive
)
if(GSSAPI_FOUND)
  target_link_libraries(httpobjecttest ${GSSAPI_LIBS})
endif()

ecm_add_test(httpfiltertest.cpp ${kioslave-http_SOURCE_DIR}/httpfilter.cpp
             TEST_NAME httpfiltertest
             LINK_LIBRARIES Qt${QT_MAJOR_VERSION}::Test KF5::I18n KF5::Archive ZLIB::ZLIB)
