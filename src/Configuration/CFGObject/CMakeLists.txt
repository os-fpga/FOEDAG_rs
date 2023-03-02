# -*- mode:cmake -*-

# Copyright 2022 The Foedag team

# GPL License

# Copyright (c) 2022 The Open-Source FPGA Foundation

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.15)
set(subsystem cfgobject)
set(test_bin ${subsystem}_test)

project(${subsystem} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
else()
  set(CMAKE_CXX_FLAGS_DEBUG
  "${CMAKE_CXX_FLAGS_DEBUG} ${TCMALLOC_COMPILE_OPTIONS} -Werror -Wall -O0 -g ${MSYS_COMPILE_OPTIONS} ${MY_CXX_WARNING_FLAGS} ${MEM_SANITIZER_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Werror")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SUBMODULE_BUILD_ROOT_DIR}/lib)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SUBMODULE_BUILD_ROOT_DIR}/bin)

#################################################################################################################
#
# Three elements will be built 
#
# 1. auto generate all the object file
#
# 2. subsystem library (in this case cfgobject) - it contains all the core code to generate configuration object (depends on item 1). It will be called by other elements
#
# 3. test_bin executable (in this case cfgobject_test exe) - it is external command line entry which eventually call subsystem library (item 1) for unit testing
#
#################################################################################################################

###################
#
# auto generate object file
#
###################
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CFGObject_auto.h
  COMMAND python3 ${PROJECT_SOURCE_DIR}/CFGObject.py ${PROJECT_SOURCE_DIR}/CFGObject.json ${CMAKE_CURRENT_BINARY_DIR}/CFGObject_auto.h
  DEPENDS ${PROJECT_SOURCE_DIR}/CFGObject.py ${PROJECT_SOURCE_DIR}/CFGObject.json
)

###################
#
# subsystem library
#
###################
add_library(
  ${subsystem} STATIC
  CFGObject.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/CFGObject_auto.h
)

###################
#
# test_bin which also has dependency on its own subsystem library
#
###################
add_executable(
  ${test_bin}
  Test/CFGObject_test.cpp
)
target_link_libraries(${test_bin} ${subsystem})

###################
#
# install 
#   - for header file(s), only put those that we really need to export
#
###################
set(
  SRC_H_INSTALL_LIST
)

set(
  SRC_LIB_INSTALL_LIST
  ${subsystem}
)

foreach(file ${SRC_LIB_INSTALL_LIST})
  install(
    FILES ${SUBMODULE_BUILD_ROOT_DIR}/lib/$<TARGET_FILE_NAME:${file}>
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/foedag
  )
endforeach()

foreach(file ${SRC_H_INSTALL_LIST})
  install(
    FILES ${PROJECT_SOURCE_DIR}/${file}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/foedag/BitGenerator
  )
endforeach()
