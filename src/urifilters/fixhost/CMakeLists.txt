kcoreaddons_add_plugin(fixhosturifilter SOURCES fixhosturifilter.cpp INSTALL_NAMESPACE "kf6/urifilters")

target_link_libraries(fixhosturifilter KF6::KIOGui)
