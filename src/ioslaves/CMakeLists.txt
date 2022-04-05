
add_subdirectory( file )
add_subdirectory( help )
add_subdirectory( http )
add_subdirectory( ftp )
add_subdirectory( telnet )
add_subdirectory( remote )

# Currently kio_trash doesn't work on Android
if (NOT ANDROID)
    add_subdirectory(trash)
endif()
