add_subdirectory(core)
add_subdirectory(widgets)
add_subdirectory(filewidgets)
add_subdirectory(kntlm)
add_subdirectory(ioslaves)
add_subdirectory(kssld)
add_subdirectory(kioslave)
add_subdirectory(kioexec)

if(NOT WIN32)
  # arpa/nameser.h
  add_subdirectory(kpac)
endif()

