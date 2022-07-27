
add_subdirectory( file )
add_subdirectory( http )
add_subdirectory( telnet )

# Currently kio_trash doesn't work on Android
if (NOT ANDROID)
    add_subdirectory(trash)
endif()
