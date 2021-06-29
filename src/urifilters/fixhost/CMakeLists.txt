kcoreaddons_add_plugin(fixhosturifilter
    INSTALL_NAMESPACE "kf5/urifilters"
)

target_sources(fixhosturifilter PRIVATE
    fixhosturifilter.cpp
)

target_link_libraries(fixhosturifilter KF5::KIOWidgets)
