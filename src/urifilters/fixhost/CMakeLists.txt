set(fixhosturifilter_PART_SRCS fixhosturifilter.cpp )

add_library(fixhosturifilter MODULE ${fixhosturifilter_PART_SRCS})

target_link_libraries(fixhosturifilter KF5::KIOWidgets)

set_target_properties(fixhosturifilter PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf5/urifilters")
install(TARGETS fixhosturifilter DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/urifilters)

########### install files ###############

install(FILES fixhosturifilter.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})

