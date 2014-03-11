set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/.. )

set (kpasswdserver_test_SRCS
 kpasswdservertest.cpp
 ../kpasswdserver.cpp
)
qt5_add_dbus_adaptor(kpasswdserver_test_SRCS ../org.kde.KPasswdServer.xml kpasswdserver.h KPasswdServer)

kde4_add_unit_test(kpasswdservertest ${kpasswdserver_test_SRCS} )
target_link_libraries(kpasswdservertest  ${KDE4_KIO_LIBRARY}
                      Qt5::Core Qt5::Test)
