include(ECMMarkAsTest)

find_package(KF5XmlGui ${KF_DEP_VERSION} REQUIRED)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

macro(KIOWIDGETS_EXECUTABLE_TESTS)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    target_link_libraries(${_testname}
       KF5::KIOCore
       KF5::KIOWidgets
       KF5::KIOFileWidgets
       Qt5::Test
       KF5::WidgetsAddons
       KF5::IconThemes)
    ecm_mark_as_test(${_testname})
  endforeach()
endmacro(KIOWIDGETS_EXECUTABLE_TESTS)

if(NOT EXCLUDE_DEPRECATED_BEFORE_AND_AT STREQUAL "CURRENT" AND
   EXCLUDE_DEPRECATED_BEFORE_AND_AT VERSION_LESS 5.71.0)
    set(runapplication_EXE runapplication)
endif()

KIOWIDGETS_EXECUTABLE_TESTS(
  getalltest
  kruntest
  kioslavetest
  kopenwithtest
  kencodingfiledialogtest_gui
  kdirmodeltest_gui
  kdirlistertest_gui
  kfilecustomdialogtest_gui
  previewtest
  kurlrequestertest_gui
  kpropertiesdialogtest
  kmountpoint_debug
  listjobtest
  listrecursivetest
  kionetrctest
  ksycocaupdatetest
  kurlnavigatortest_gui
  kprotocolinfo_dumper
  kfilewidgettest_gui
  kfilewidgettest_saving_gui
  openfilemanagerwindowtest
  ${runapplication_EXE}
)
