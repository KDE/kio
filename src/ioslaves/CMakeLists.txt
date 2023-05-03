
add_subdirectory( file )

# Currently kio_trash doesn't work on Android
if (NOT ANDROID AND NOT WIN32)
    add_subdirectory(trash)
endif()
