project(KIONTLM)

set(kntlm_LIB_SRCS kntlm.cpp des.cpp )

add_library(KF5KIONTLM ${kntlm_LIB_SRCS})
generate_export_header(KF5KIONTLM BASE_NAME KNTLM)
add_library(KF5::KIONTLM ALIAS KF5KIONTLM)

target_link_libraries(KF5KIONTLM KF5::CoreAddons)

set_target_properties(KF5KIONTLM PROPERTIES VERSION ${KIO_VERSION_STRING}
                                            SOVERSION ${KIO_SOVERSION}
                                            EXPORT_NAME KIONTLM
                                            )

install(TARGETS KF5KIONTLM EXPORT KF5KIOTargets  ${INSTALL_TARGETS_DEFAULT_ARGS})

target_include_directories(KF5KIONTLM PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>" # kio_version.h
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>" # kio/global.h
  #"$<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}/KDE>" # KIO/Job
)

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/kntlm_export.h kntlm.h  DESTINATION  ${INCLUDE_INSTALL_DIR}/kio COMPONENT Devel)

