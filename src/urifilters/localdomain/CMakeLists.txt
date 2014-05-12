set(localdomainurifilter_PART_SRCS localdomainurifilter.cpp )

add_library(localdomainurifilter MODULE ${localdomainurifilter_PART_SRCS})

target_link_libraries(localdomainurifilter KF5::KIOWidgets)

install(TARGETS localdomainurifilter DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install(FILES localdomainurifilter.desktop DESTINATION ${SERVICES_INSTALL_DIR})
