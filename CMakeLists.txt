# find_package(Strigi)
# set_package_properties(Strigi PROPERTIES DESCRIPTION "Desktop indexing and search support"
#                        URL "http://strigi.sourceforge.net"
#                        TYPE ${STRIGI_TYPE}
#                       )
# if (WIN32)
#     set (STRIGI_TYPE "OPTIONAL")
# else ()
#     set (STRIGI_TYPE "REQUIRED")
# endif ()

add_definitions(-DTRANSLATION_DOMAIN=\"kio_trash\")
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0)

add_subdirectory(tests)

set (trashcommon_PART_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/trashimpl.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/discspaceutil.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/trashsizecache.cpp
                           ${CMAKE_CURRENT_SOURCE_DIR}/kinterprocesslock.cpp
    )

########### next target ###############
if(WIN32)
  set(kio_trash_PART_SRCS kio_trash_win.cpp kio_trash_win.h)
else()
  set(kio_trash_PART_SRCS kio_trash.cpp ${trashcommon_PART_SRCS})
endif()
add_library(kio_trash MODULE ${kio_trash_PART_SRCS})

target_link_libraries(kio_trash
	KF5::Solid
        KF5::KDE4Support
	KF5::KIOCore
        Qt5::DBus
        KF5::I18n KF5::ConfigCore KF5::ConfigGui 
        )
install(TARGETS kio_trash  DESTINATION ${PLUGIN_INSTALL_DIR} )

set(ktrash_SRCS ktrash.cpp )

add_executable(ktrash5 ${ktrash_SRCS})
ecm_mark_nongui_executable(ktrash5)

target_link_libraries(ktrash5
        KF5::KDE4Support
	KF5::KIOCore
        KF5::I18n KF5::ConfigCore KF5::ConfigGui
        )

install(TARGETS ktrash5 ${INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

# currently not on win32, TODO!
if(NOT WIN32)
    set(kcm_trash_PART_SRCS kcmtrash.cpp ${trashcommon_PART_SRCS})
    add_library(kcm_trash MODULE ${kcm_trash_PART_SRCS})
    target_link_libraries(kcm_trash  Qt5::DBus  KF5::KDE4Support
        ${KDECLARATIVE_LIBRARIES} KF5::I18n KF5::ConfigWidgets KF5::KIOCore KF5::Solid)

    install(TARGETS kcm_trash DESTINATION ${PLUGIN_INSTALL_DIR})
endif()

########### install files ###############

install( FILES trash.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kcmtrash.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
