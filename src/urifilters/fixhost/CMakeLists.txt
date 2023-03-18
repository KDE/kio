kcoreaddons_add_plugin(fixhosturifilter
    INSTALL_NAMESPACE "kf6/urifilters"
)

target_sources(fixhosturifilter PRIVATE
    fixhosturifilter.cpp
)

target_link_libraries(fixhosturifilter KF6::KIOWidgets)

if (ENABLE_PCH)
    target_precompile_headers(fixhosturifilter REUSE_FROM KIOPchWidgets)
endif()
