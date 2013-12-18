project(kioslave-help)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)

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

find_package(Qt5Core 5.2.0 REQUIRED NO_MODULE)

########### next target ###############

set(kio_help_PART_SRCS
    kio_help.cpp
    main.cpp
    xslt_help.cpp
)


add_library(kio_help MODULE ${kio_help_PART_SRCS})

target_link_libraries(kio_help
   Qt5::Gui # QTextDocument
   KF5::I18n
   KF5::Archive
   KF5::KIOCore
   KF5::XsltKde
   ${LIBXML2_LIBRARIES}
   ${LIBXSLT_LIBRARIES}
   ${LIBXSLT_EXSLT_LIBRARIES}
)

install(TARGETS kio_help  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############

set(kio_ghelp_PART_SRCS kio_help.cpp main_ghelp.cpp xslt_help.cpp)


add_library(kio_ghelp MODULE ${kio_ghelp_PART_SRCS})

target_link_libraries(kio_ghelp
   Qt5::Gui # QTextDocument
   KF5::I18n
   KF5::Archive
   KF5::KIOCore
   KF5::XsltKde
   ${LIBXML2_LIBRARIES}
   ${LIBXSLT_LIBRARIES}
   ${LIBXSLT_EXSLT_LIBRARIES}
)

install(TARGETS kio_ghelp  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES help.protocol ghelp.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )

