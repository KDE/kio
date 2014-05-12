set(kurifiltertest_SRCS kurifiltertest.cpp)

add_executable(kurifiltertest ${kurifiltertest_SRCS})
target_link_libraries(kurifiltertest KF5::KIOWidgets Qt5::Test)

ecm_mark_as_test(kurifiltertest)
add_test(kurifiltertest kurifiltertest)
