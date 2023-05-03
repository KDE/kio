
add_subdirectory( file )

# Currently kio_trash doesn't work on Android
if (NOT ANDROID)
    add_subdirectory(trash)
endif()
