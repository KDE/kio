set(fixhosturifilter_SRCS fixhosturifilter.cpp )

kcoreaddons_add_plugin(fixhosturifilter INSTALL_NAMESPACE "kf5/urifilters" JSON fixhosturifilter.json SOURCES ${fixhosturifilter_SRCS})

target_link_libraries(fixhosturifilter KF5::KIOWidgets)
