# kio_help only buildable if doc tools around
if (KF5DocTools_FOUND)

project(kioslave-help)

find_package(KF5Archive ${KF_DEP_VERSION} REQUIRED)
find_package(LibXslt)
set_package_properties(LibXslt PROPERTIES
                       URL "http://xmlsoft.org/XSLT"
                       TYPE REQUIRED
                       PURPOSE "Required by the KDE help system to process DocBook XML"
                      )

find_package(LibXml2)
set_package_properties(LibXml2 PROPERTIES
                       URL "http://xmlsoft.org"
                       TYPE REQUIRED
                       PURPOSE "Required by the KDE help system to process DocBook XML"
                      )

if(NOT LIBXML2_XMLLINT_EXECUTABLE)
  message(FATAL_ERROR "xmllint is required by the KDE help system to process DocBook XML")
endif()

include_directories(${LIBXML2_INCLUDE_DIR} ${LIBXSLT_INCLUDE_DIR})

include(ConfigureChecks.cmake)
configure_file(config-help.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-help.h )

#macro_additional_clean_files( ${CMAKE_CURRENT_BINARY_DIR}/checkXML )

find_package(Qt5Core ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

########### next target ###############

set(kio_help_PART_SRCS
    kio_help.cpp
    main.cpp
    xslt_help.cpp
)


add_library(kio_help MODULE ${kio_help_PART_SRCS})

target_link_libraries(kio_help
   Qt5::Gui # QTextDocument
   Qt5::Network
   KF5::I18n
   KF5::Archive
   KF5::KIOCore
   KF5::DocTools
   ${LIBXML2_LIBRARIES}
   ${LIBXSLT_LIBRARIES}
   ${LIBXSLT_EXSLT_LIBRARIES}
)

set_target_properties(kio_help PROPERTIES OUTPUT_NAME "help")
set_target_properties(kio_help PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio")
install(TARGETS kio_help  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)


########### next target ###############

set(kio_ghelp_PART_SRCS kio_help.cpp main_ghelp.cpp xslt_help.cpp)


add_library(kio_ghelp MODULE ${kio_ghelp_PART_SRCS})

target_link_libraries(kio_ghelp
   Qt5::Gui # QTextDocument
   KF5::I18n
   KF5::Archive
   KF5::KIOCore
   KF5::DocTools
   Qt5::Network
   ${LIBXML2_LIBRARIES}
   ${LIBXSLT_LIBRARIES}
   ${LIBXSLT_EXSLT_LIBRARIES}
)
if(WIN32)
    # libxml/libxslt pulls in windows.h, this breaks QDateTime
    target_compile_definitions(kio_help PRIVATE NOMINMAX)
    target_compile_definitions(kio_ghelp PRIVATE NOMINMAX)
endif()
set_target_properties(kio_ghelp PROPERTIES OUTPUT_NAME "ghelp")
set_target_properties(kio_ghelp PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/kio")
install(TARGETS kio_ghelp  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)

endif()
