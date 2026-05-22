# SPDX-FileCopyrightText: 2026 Friedrich W. H. Kossebau <kossebau@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause

#[=======================================================================[.rst:
ECMInstalledLibraryCheck (PREVIEW 2026-06-30, shared for feedback)
------------------------------------------------------------------

.. note::
    This is a copy shared for the purpose of testing and gathering feedback
    on the usefulness of the current macros.
    Please report your thoughts and ideas at:
    https://invent.kde.org/frameworks/extra-cmake-modules/-/merge_requests/590

Generates a check to test artifacts of the library installation, like the
self-containedness of the CMake config files as well as of the official public
headers in the deployed layout.

::

  ecm_add_installed_library_check(<library_target>
      [PACKAGE_NAME <package_name>]
      [PACKAGE_VERSION <package_version>]
      [PACKAGE_TARGET_NAMESPACE <package_target_namespace>]
      [NO_PACKAGE_TARGET_NAMESPACE]
      [COMPILE_DEFINITIONS <definition> [...]]
      [EXTRA_DEPENDENCIES <dependency> [<version>] [...]]
      [EXTRA_LINK_LIBRARIES <library> [...]])

The function creates a target "<library_target>_installed_library_check" which
can be invoked after the installation to check if the installed library artifacts
are self-contained when used from a consumer.
All these targets are added as dependency to a target
"all_installed_library_check", which is created at the level where this module
is first included.
The check generates a CMake project with a dummy library which searches the given
package for any specified version, links to the exported target as imported from
the package's Cmake config file and as its sources for each given include strings
adds a C++ source file with just the content "#include <include_string>".
This project then is configured with cmake and all these sources are built.

``PACKAGE_NAME`` specifies the name of the CMake package to check for.
The default is "${PROJECT_NAME}".

``PACKAGE_VERSION`` specifies the version of the CMake package to check for.
The default is "${PROJECT_VERSION}" if set, otherwise none.

``PACKAGE_TARGET_NAMESPACE`` specifies what namespace the exported target
name of the library is placed in. The default is the value estimated for
the package name, unless ``NO_PACKAGE_TARGET_NAMESPACE`` is set.

``NO_PACKAGE_TARGET_NAMESPACE`` defines that the library target is exported
in the package without any namespace.

``COMPILE_DEFINITIONS`` can be used to set custom definitions for the test
builds against the headers.

``EXTRA_DEPENDENCIES`` can be used to add custom dependencies to search for
with "find_package()`. This can be used if the current dependencies declared
in the installed CMake config are not complete, but can not be changed.

``EXTRA_LINK_LIBRARIES`` can be used to add custom libraries to link to with
"target_link_libraries()`. This can be used if the current list of libraries
in the public interface is not complete, but can not be changed.

::

  ecm_installed_library_check_include_strings(<library_target>
      HEADERS <package_name>
      [PREFIX <prefix>])


``HEADERS`` specifies the header files whose base names will be available as
public include strings.

``PREFIX`` specifies a prefix, which consumers need to append next to the base names
of the headers passed to ``HEADERS``. The argument <prefix> is specified without a
trailing "/". Default is none.

::

  ecm_installed_library_check_cmake_variable(<library_target>
      NAME <name>
      VALUE <value>
      [TYPE <type>])


``NAME`` specifies the name of a variable expected to be set after finding the
package.

``VALUE`` specifies the value expected to be set for the variable. For variables
of the type "BOOL" the usual evaluation of CMake to a boolean value is compared.

``TYPE`` specifies the type of the variable. The options are "BOOL", "STRING".
Default is "STRING"

::

  ecm_installed_library_check_compile_definition(<library_target>
      NAME <name>
      [VALUE <value>]
      [UNDEFINED])


``NAME`` specifies the name of a macro to test.

``VALUE`` specifies the value expected to be set for the macro If not passed,
the macro is only tested for being defined. Only numeric values are supported.

``UNDEFINED`` specifies if the macro should be tested for being undefined.

::

  ecm_installed_library_check_preprocessor_macro(<library_target>
      NAME <name>
      [VALUE <value>]
      [UNDEFINED]
      [SILENT])


``NAME`` specifies the name of a macro to test.

``VALUE`` specifies the value expected to be set for the macro If not passed,
the macro is only tested for being defined. Only numeric values are supported.

``UNDEFINED`` specifies if the macro should be tested for being undefined.

``SILENT`` specifies if the positive result should not be reported.

::

  ecm_installed_library_check_version_preprocessor_macros(<library_target>
      PREFIX <prefix>
      [VERSION <version>]
      [SILENT])


``PREFIX`` specifies the prefix of the version macros to test. The expected macro
names are: "<prefix>_VERSION" (hexadecimal number), "<prefix>_VERSION_MAJOR",
"<prefix>_VERSION_MINOR" and "<prefix>_VERSION_PATCH".

``VERSION`` specifies the version string "<major>.<minor>.<patch>".

``SILENT`` specifies if the positive results should not be reported.

Example usage:

.. code-block:: cmake

  # add a non-default target "MyLib_installed_library_check",
  # which will test for a CMake config file for "MyPackage",
  # at version "1.0",  with the imported library target
  # "MyPackage::MyLib" and whose include strings (headers)
  # can be used self-contained when linking the target
  ecm_add_installed_library_check(MyLib
      PACKAGE_NAME "MyPackage"
      PACKAGE_VERSION "1.0"
  )

  # for any <MLFoo> etc. includes
  ecm_installed_library_check_include_strings(MyLib
      HEADERS
          /absolute/path/MLFoo
          relative/path/MLBar
          # etc
  )

  # for any <ML/Foo> etc. includes
  ecm_installed_library_check_include_strings(MyLib
      HEADERS
          /absolute/path/Foo
          relative/path/Bar
          # etc
      PREFIX ML
  )

Since 6.29
#]=======================================================================]

cmake_policy(VERSION 3.29)

if(NOT TARGET all_installed_library_check)
    add_custom_target(all_installed_library_check)
endif()

# Purpose: testing basic self-containedness of public headers and cmake config files
# no testing of actual symbols or template methods
# TODO: support testing with other build metadata formats, like pkgconfig?
function(ecm_add_installed_library_check _target)
    set(options NO_PACKAGE_TARGET_NAMESPACE)
    set(oneValueArgs PACKAGE_NAME PACKAGE_VERSION PACKAGE_TARGET_NAMESPACE)
    set(multiValueArgs EXTRA_DEPENDENCIES EXTRA_LINK_LIBRARIES COMPILE_DEFINITIONS)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_add_installed_library_check(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(ARG_NO_PACKAGE_TARGET_NAMESPACE AND ARG_PACKAGE_TARGET_NAMESPACE)
        message(FATAL_ERROR "ecm_add_installed_library_check cannot be called with both NO_PACKAGE_TARGET_NAMESPACE and PACKAGE_TARGET_NAMESPACE args.")
    endif()
    set(_library_types "STATIC_LIBRARY" "SHARED_LIBRARY" "INTERFACE_LIBRARY")
    get_target_property(_type ${_target} TYPE)
    if(NOT ${_type} IN_LIST _library_types)
        message(FATAL_ERROR "ecm_add_installed_library_check cannot be called on a target which is not some library. Was: ${_type}")
    endif()

    # setup data
    if(ARG_PACKAGE_NAME)
        set(_package_name "${ARG_PACKAGE_NAME}")
    else()
        set(_package_name "${PROJECT_NAME}")
    endif()
    if(ARG_PACKAGE_VERSION)
        set(_package_version ${ARG_PACKAGE_VERSION})
    else()
        if(PROJECT_VERSION)
            set(_package_version ${PROJECT_VERSION})
        else()
            set(_package_version)
        endif()
    endif()
    if(_package_version)
        set(_package_version_exact "EXACT")
    else()
        set(_package_version_exact)
    endif()
    if(ARG_PACKAGE_TARGET_NAMESPACE)
        set(_library_target_namespace ${ARG_PACKAGE_TARGET_NAMESPACE})
    else()
        if(ARG_NO_PACKAGE_TARGET_NAMESPACE)
            set(_library_target_namespace)
        else()
            set(_library_target_namespace "${_package_name}::")
        endif()
    endif()
    set(_extra_find_packages_statements)
    if(ARG_EXTRA_DEPENDENCIES)
        list(LENGTH ARG_EXTRA_DEPENDENCIES _deps_count)
        set(_i 0)
        while(${_i} LESS ${_deps_count})
            list(GET ARG_EXTRA_DEPENDENCIES ${_i} _extra_package_name)
            math(EXPR _i "${_i} + 1")
            set(_extra_package_version_string)
            # check next arg if it is version and thus to be consumed
            if(${_i} LESS ${_deps_count})
                list(GET ARG_EXTRA_DEPENDENCIES ${_i} _extra_package_version)
                if(_extra_package_version MATCHES "^([0-9]+)\\.([0-9]+)(\\.([0-9]+))?$")
                    set(_extra_package_version_string " ${_extra_package_version}")
                    math(EXPR _i "${_i} + 1")
                endif()
            endif()
            set(_extra_find_packages_statements "find_package(${_extra_package_name}${_extra_package_version_string} REQUIRED)\n")
        endwhile()
    endif()
    set(_extra_link_libraries_list)
    foreach(_link_library IN LISTS ARG_EXTRA_LINK_LIBRARIES)
        string(APPEND _extra_link_libraries_list "\n    ${_link_library}")
    endforeach()
    set(_compile_definitions_statement)
    if(ARG_COMPILE_DEFINITIONS)
        set(_compile_definitions_statement "target_compile_definitions(InstalledLibraryCheck PRIVATE ${ARG_COMPILE_DEFINITIONS})\n")
    endif()
    set(_prefix_path "${CMAKE_INSTALL_PREFIX}")

    set(_compiler_propagation)
    foreach(_compiler IN ITEMS CMAKE_C_COMPILER CMAKE_CXX_COMPILER)
        if (${_compiler})
            list(APPEND _compiler_propagation "-D${_compiler}=${${_compiler}}")
        endif()
    endforeach()

    set(_languages)
    if (CMAKE_C_COMPILER)
        string(APPEND _languages " C")
    endif()
    if (CMAKE_CXX_COMPILER)
        string(APPEND _languages " CXX")
    endif()
    if (_languages)
        string(PREPEND _languages " LANGUAGES")
    endif()

    # prepare (generator) expressions
    set(_installed_library_check_dir "${CMAKE_CURRENT_BINARY_DIR}/${_target}_ECMInstalledLibraryCheck")
    set(_include_strings "$<TARGET_PROPERTY:${_target},ECM_INSTALLED_LIBRARY_INCLUDE_STRINGS>")
    set(_include_strings_sorted "$<LIST:SORT,${_include_strings}>")
    set(_include_strings_lines "$<JOIN:${_include_strings_sorted},\n    >")
    set(_exported_target_name "$<TARGET_PROPERTY:${_target},EXPORT_NAME>")
    set(_library_target_name "${_library_target_namespace}$<IF:$<BOOL:${_exported_target_name}>,${_exported_target_name},${_target}>")

    set(_cmake_variables_check_statements "$<TARGET_PROPERTY:${_target},ECM_INSTALLED_LIBRARY_CMAKE_VARIABLES_CHECK_CODE>")

    set(_compile_definitions_check_statements "$<TARGET_PROPERTY:${_target},ECM_INSTALLED_LIBRARY_COMPILE_DEFINITIONS_CHECK_CODE>")
    set(_source_file "ecm_installed_library_check_compile_definitions.cpp")
    set(_compile_definitions_check_code
"$<IF:$<BOOL:${_compile_definitions_check_statements}>,
file(GENERATE OUTPUT ${_source_file} CONTENT \"${_compile_definitions_check_statements}\")\n
target_sources(InstalledLibraryCheck PRIVATE ${_source_file})\n,>")

    set(_preprocessor_macro_check_statements "$<TARGET_PROPERTY:${_target},ECM_INSTALLED_LIBRARY_PREPROCESSOR_MACRO_CHECK_CODE>")
    set(_preprocessor_macro_check_code
"$<IF:$<BOOL:${_preprocessor_macro_check_statements}>,
\"${_preprocessor_macro_check_statements}\",>")

    file(GENERATE
        OUTPUT "${_installed_library_check_dir}/CMakeLists.txt"
        CONTENT
"# This file was generated by ecm_add_installed_library_check(). DO NOT EDIT!
cmake_minimum_required(VERSION 3.29)

project(InstalledLibraryCheck${_languages})

find_package(${_package_name} ${_package_version} ${_package_version_exact} CONFIG REQUIRED NO_DEFAULT_PATH
    PATHS \"${_prefix_path}\"
)
${_extra_find_packages_statements}
include(FeatureSummary)
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
${_cmake_variables_check_statements}
add_library(InstalledLibraryCheck MODULE)
target_link_libraries(InstalledLibraryCheck
    ${_library_target_name}${_extra_link_libraries_list}
)
${_compile_definitions_statement}
set(_include_strings
    ${_include_strings_lines}
)
${_compile_definitions_check_code}
set(_preprocessor_macro_check_statements${_preprocessor_macro_check_code})

foreach(_include_string IN LISTS _include_strings)
    string(REPLACE \"/\" \"__\" _escaped_include_string \${_include_string})
    set(_source_file \"\${CMAKE_CURRENT_BINARY_DIR}/\${_escaped_include_string}.cpp\")
    file(GENERATE OUTPUT \${_source_file} CONTENT \"#include <\${_include_string}>\\n\${_preprocessor_macro_check_statements}\")
    target_sources(InstalledLibraryCheck PRIVATE \${_source_file})
endforeach()
"
    )

    set(_check_target_name ${_target}_installed_library_check)
    add_custom_target(${_check_target_name}
        COMMAND ${CMAKE_COMMAND}
            # TODO: other options to pass on? e.g. for cross-compilation, tool-chain file?
            -G ${CMAKE_GENERATOR}
            ${_compiler_propagation}
            --fresh
            .
        COMMAND ${CMAKE_COMMAND} --build .
        WORKING_DIRECTORY ${_installed_library_check_dir}
        JOB_SERVER_AWARE
    )

    add_dependencies(all_installed_library_check ${_check_target_name})
endfunction()

function(ecm_installed_library_check_include_strings _target)
    set(options)
    set(oneValueArgs PREFIX)
    set(multiValueArgs HEADERS)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_installed_library_check_include_strings(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()

    # store data with library target
    get_target_property(_names ${_target} ECM_INSTALLED_LIBRARY_INCLUDE_STRINGS)
    if(_names STREQUAL "_names-NOTFOUND")
        set(_names)
    endif()

    if(ARG_PREFIX)
        string(APPEND ARG_PREFIX "/")
    endif()
    foreach(_header IN LISTS ARG_HEADERS)
        cmake_path(GET _header FILENAME _name)
        list(APPEND _names "${ARG_PREFIX}${_name}")
    endforeach()

    set_target_properties(${_target} PROPERTIES ECM_INSTALLED_LIBRARY_INCLUDE_STRINGS "${_names}")
endfunction()

function(ecm_installed_library_check_cmake_variable _target)
    set(options)
    set(oneValueArgs NAME TYPE VALUE)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_installed_library_check_cmake_variable(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "Missing NAME argument to ecm_installed_library_check_cmake_variable(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(ARG_TYPE)
        set(_variable_types "BOOL" "STRING")
        if(NOT ${ARG_TYPE} IN_LIST _variable_types)
            message(FATAL_ERROR "ecm_installed_library_check_cmake_variable called with unknown type: ${ARG_TYPE}")
        endif()
    else()
        set(ARG_TYPE "STRING")
    endif()

    # store code with library target
    get_target_property(_code ${_target} ECM_INSTALLED_LIBRARY_CMAKE_VARIABLES_CHECK_CODE)
    if(_code STREQUAL "_code-NOTFOUND")
        set(_code "\nmessage(STATUS \"Checking CMake variables\")\n")
    endif()

    if(${ARG_TYPE} STREQUAL "BOOL")
        if (${ARG_VALUE})
            set(_value_condition_code "\${${ARG_NAME}}")
            set(_value_error_report "is FALSE, expecting TRUE")
        else()
            set(_value_condition_code "NOT \${${ARG_NAME}}")
            set(_value_error_report "is TRUE, expecting FALSE")
        endif()
    else() # STRING
        set(_value_condition_code "\${${ARG_NAME}} STREQUAL \"${ARG_VALUE}\"")
        set(_value_error_report "is '\${${ARG_NAME}}', expecting '${ARG_VALUE}'")
    endif()

    set(_var_check_code
"if(NOT DEFINED ${ARG_NAME})
    message(SEND_ERROR \"${ARG_NAME}: not defined\")
else()
    if (${_value_condition_code})
        message(STATUS \"${ARG_NAME}: passed\")
    else()
        message(SEND_ERROR \"${ARG_NAME}: ${_value_error_report}\")
    endif()
endif()
")

    string(APPEND _code "${_var_check_code}")

    set_target_properties(${_target} PROPERTIES ECM_INSTALLED_LIBRARY_CMAKE_VARIABLES_CHECK_CODE "${_code}")
endfunction()

function(_ecm_installed_library_check_get_check_code _target _var_name _property_name _subject_name _silent)
    get_target_property(_check_code ${_target} ${_property_name})
    if(_check_code STREQUAL "_check_code-NOTFOUND")
        if (_silent)
            set(_check_code "")
        else()
            set(_check_code "#pragma message(\\\"Checking ${_subject_name}\\\")\n")
        endif()
        string(APPEND _check_code
"#define _ECM_INSTALLED_LIBRARY_CHECK_VALUE_TO_STRING(x) #x
#define _ECM_INSTALLED_LIBRARY_CHECK_VALUE(x) _ECM_INSTALLED_LIBRARY_CHECK_VALUE_TO_STRING(x)
")
    endif()
    set(${_var_name} ${_check_code} PARENT_SCOPE)
endfunction()

function(_ecm_installed_library_check_macro_value_check_code _var_name _name _value _silent)
    set(_check_code
"#if !defined(${_name})
    #error(\\\"${_name}: not defined\\\")
#else
    #if ${_name} != ${_value}
        #pragma message(\\\"${_name}: is '\\\" _ECM_INSTALLED_LIBRARY_CHECK_VALUE(${_name}) \\\"'\\\")
        #error(\\\"${_name}: expecting '${_value}'\\\")
")
    if (NOT _silent)
        string(APPEND _check_code
"    #else
       #pragma message(\\\"${_name}: passed\\\")
")
    endif()
    string(APPEND _check_code
"    #endif
#endif
")
    set(${_var_name} ${_check_code} PARENT_SCOPE)
endfunction()

function(_ecm_installed_library_check_macro_defined_check_code _var_name _name _silent)
    set(_check_code
"#if !defined(${_name})
    #error(\\\"${_name}: is undefined, expecting defined\\\")
")
    if (NOT _silent)
        string(APPEND _check_code
"#else
    #pragma message(\\\"${_name}: passed\\\")
")
    endif()
    string(APPEND _check_code
"#endif
")

    set(${_var_name} ${_check_code} PARENT_SCOPE)
endfunction()

function(_ecm_installed_library_check_macro_undefined_check_code _var_name _name _silent)
    set(_check_code
"#if defined(${_name})
    #error(\\\"${_name}: is defined, expecting undefined\\\")
")
    if (NOT _silent)
        string(APPEND _check_code
"#else
    #pragma message(\\\"${_name}: passed\\\")
")
    endif()
    string(APPEND _check_code
"#endif
")
    set(${_var_name} ${_check_code} PARENT_SCOPE)
endfunction()

function(ecm_installed_library_check_compile_definition _target)
    set(options UNDEFINED)
    set(oneValueArgs NAME VALUE)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_installed_library_check_compile_definition(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "Missing NAME argument to ecm_installed_library_check_compile_definition(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(ARG_VALUE AND ARG_UNDEFINED)
        message(FATAL_ERROR "ecm_installed_library_check_compile_definition cannot be called with both VALUE and UNDEFINED args.")
    endif()

    # store code with library target
    _ecm_installed_library_check_get_check_code(${_target} _code
        ECM_INSTALLED_LIBRARY_COMPILE_DEFINITIONS_CHECK_CODE "compile definitions" FALSE)

    if(DEFINED ARG_VALUE)
        _ecm_installed_library_check_macro_value_check_code(_var_check_code ${ARG_NAME} ${ARG_VALUE} FALSE)
    elseif(ARG_UNDEFINED)
        _ecm_installed_library_check_macro_undefined_check_code(_var_check_code ${ARG_NAME} FALSE)
    else()
        _ecm_installed_library_check_macro_defined_check_code(_var_check_code ${ARG_NAME} FALSE)
    endif()

    string(APPEND _code "${_var_check_code}")

    set_target_properties(${_target} PROPERTIES ECM_INSTALLED_LIBRARY_COMPILE_DEFINITIONS_CHECK_CODE "${_code}")
endfunction()

function(ecm_installed_library_check_preprocessor_macro _target)
    set(options UNDEFINED SILENT)
    set(oneValueArgs NAME VALUE)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_installed_library_check_preprocessor_macro(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "Missing NAME argument to ecm_installed_library_check_preprocessor_macro(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(ARG_VALUE AND ARG_UNDEFINED)
        message(FATAL_ERROR "ecm_installed_library_check_preprocessor_macro cannot be called with both VALUE and UNDEFINED args.")
    endif()

    # store code with library target
    _ecm_installed_library_check_get_check_code(${_target} _code
        ECM_INSTALLED_LIBRARY_PREPROCESSOR_MACRO_CHECK_CODE "preprocessor macros" ${ARG_SILENT})

    if (ARG_SILENT)
        set(_silent TRUE)
    else()
        set(_silent FALSE)
    endif()

    if(DEFINED ARG_VALUE)
        _ecm_installed_library_check_macro_value_check_code(_var_check_code ${ARG_NAME} ${ARG_VALUE} ${_silent})
    elseif(ARG_UNDEFINED)
        _ecm_installed_library_check_macro_undefined_check_code(_var_check_code ${ARG_NAME} ${_silent})
    else()
        _ecm_installed_library_check_macro_defined_check_code(_var_check_code ${ARG_NAME} ${_silent})
    endif()

    string(APPEND _code "${_var_check_code}")

    set_target_properties(${_target} PROPERTIES ECM_INSTALLED_LIBRARY_PREPROCESSOR_MACRO_CHECK_CODE "${_code}")
endfunction()

function(ecm_installed_library_check_version_preprocessor_macros _target)
    set(options SILENT)
    set(oneValueArgs PREFIX VERSION)
    set(multiValueArgs)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # argument checks
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to ecm_installed_library_check_version_preprocessor_macros(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()
    if(NOT DEFINED ARG_PREFIX)
        message(FATAL_ERROR "Missing NAME argument to ecm_installed_library_check_version_preprocessor_macros(): \"${ARG_UNPARSED_ARGUMENTS}\"")
    endif()

    if(NOT DEFINED ARG_VERSION)
        set(ARG_VERSION "${PROJECT_VERSION}")
    endif()
    if(ARG_SILENT)
        set(_silent SILENT)
    else()
        set(_silent)
    endif()

    string(REGEX MATCH  "^0*([0-9]+)\\.0*([0-9]+)\\.0*([0-9]+)$" _ ${ARG_VERSION})
    set(_version_major ${CMAKE_MATCH_1})
    set(_version_minor ${CMAKE_MATCH_2})
    set(_version_patch ${CMAKE_MATCH_3})

    math(EXPR _hex_number "${_version_major}*65536 + ${_version_minor}*256 + ${_version_patch}" OUTPUT_FORMAT HEXADECIMAL)

    ecm_installed_library_check_preprocessor_macro(${_target}
        NAME ${ARG_PREFIX}_VERSION
        VALUE ${_hex_number}
        ${_silent}
    )
    ecm_installed_library_check_preprocessor_macro(${_target}
        NAME ${ARG_PREFIX}_VERSION_MAJOR
        VALUE ${_version_major}
        ${_silent}
    )
    ecm_installed_library_check_preprocessor_macro(${_target}
        NAME ${ARG_PREFIX}_VERSION_MINOR
        VALUE ${_version_minor}
        ${_silent}
    )
    ecm_installed_library_check_preprocessor_macro(${_target}
        NAME ${ARG_PREFIX}_VERSION_PATCH
        VALUE ${_version_patch}
        ${_silent}
    )
endfunction()
