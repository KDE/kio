set(fixhosturifilter_PART_SRCS fixhosturifilter.cpp )

add_library(fixhosturifilter MODULE ${fixhosturifilter_PART_SRCS})

target_link_libraries(fixhosturifilter KF5::KIOWidgets)

install(TARGETS fixhosturifilter DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install(FILES fixhosturifilter.desktop DESTINATION ${SERVICES_INSTALL_DIR})

