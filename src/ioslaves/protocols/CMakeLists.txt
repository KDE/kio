
install( FILES
   data.protocol
   rtsp.protocol
   mms.protocol
   mmst.protocol
   mmsu.protocol
   pnm.protocol
   rtspt.protocol
   rtspu.protocol
   DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )

# Local copy for the unittests
add_custom_target(copy_protocols)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/kservices5)
add_custom_command(TARGET copy_protocols PRE_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/bin/kservices5) # KProtocolInfoFactory looks in that relative path
add_dependencies(KF5KIOCore copy_protocols)

