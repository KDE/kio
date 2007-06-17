
project(kioslave-trash)

include_directories(${STRIGI_INCLUDE_DIR})

set (trashcommon_PART_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp)

########### next target ###############

set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${kio_trash_PART_SRCS})

kde4_add_plugin(kio_trash ${kio_trash_PART_SRCS})


target_link_libraries(kio_trash  ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS})

install(TARGETS kio_trash  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############

kde4_add_library(trash MODULE trashthroughanalyzer.cpp trashimpl.cpp)
target_link_libraries(trash ${STREAMANALYZER_LIBRARY} ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS})
set_target_properties(trash PROPERTIES
    PREFIX strigita_)
#install(TARGETS trash LIBRARY DESTINATION ${LIB_INSTALL_DIR}/strigi)

########### next target ###############

set(ktrash_SRCS ktrash.cpp )

kde4_automoc(${ktrash_SRCS})

kde4_add_executable(ktrash NOGUI ${ktrash_SRCS})

target_link_libraries(ktrash  ${KDE4_KIO_LIBS} )

install(TARGETS ktrash DESTINATION ${BIN_INSTALL_DIR})

########### next target ###############

set(testtrash_SRCS testtrash.cpp ${trashcommon_PART_SRCS})

kde4_automoc(${testtrash_SRCS})

kde4_add_unit_test(testtrash ${testtrash_SRCS})

target_link_libraries(testtrash  ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS})

########### install files ###############

install( FILES trash.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )



