cmake_minimum_required(VERSION 3.15)

set(cfg_path ${CMAKE_CURRENT_SOURCE_DIR})
project(configuration)

add_custom_target(configuration
	COMMAND
		echo "This is top level build for Configuration subsystem. It does not really build any target but it defines all subsystem or subdirectory and their dependencies"
	)

add_custom_target(pre-configuration
	COMMAND
		echo "This is top level build for Pre-Configuration subsystem. It does not really build any target but it defines all subsystem or subdirectory that need to be built in configuration first"
	)

list(APPEND NO-COMMON-PRE-SUBSYSTEMS CFGObject)
list(APPEND PRE-SUBSYSTEMS CFGCommon ${NO-COMMON-PRE-SUBSYSTEMS})
list(APPEND POST-SUBSYSTEMS BitAssembler BitGenerator)
list(APPEND SUBSYSTEMS ${PRE-SUBSYSTEMS} ${POST-SUBSYSTEMS})

# set parent scope directory definition before subdirectory is added so that it can be inherited
#	in each subdirectory avoid using using ".." relative path -- it is very hard to manage if we have multiple layer
set(SUBMODULE_PROJECT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../..)
set(SUBMODULE_BUILD_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../..)
message("Configuration source dirctory is ${PROJECT_SOURCE_DIR}")
message("Configuration binary dirctory is ${CMAKE_CURRENT_BINARY_DIR}")
message("Configuration set project root directory as ${SUBMODULE_PROJECT_ROOT_DIR}")
message("Configuration set build root directory as ${SUBMODULE_BUILD_ROOT_DIR}")

# subdirectory
foreach(SUBSYSTEM ${SUBSYSTEMS})
	add_subdirectory(${SUBSYSTEM})
endforeach()

# include configuration to all subsystem
foreach(SUBSYSTEM ${SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	target_include_directories(${SUBSYSTEM} PUBLIC
        ${SUBMODULE_PROJECT_ROOT_DIR}/FOEDAG/src
        ${SUBMODULE_PROJECT_ROOT_DIR}/src
        ${SUBMODULE_PROJECT_ROOT_DIR}/src/Configuration
        ${SUBMODULE_BUILD_ROOT_DIR}/include
        ${SUBMODULE_BUILD_ROOT_DIR}/src
        ${SUBMODULE_BUILD_ROOT_DIR}/src/Configuration
    )
endforeach()

# default dependencies on pre-configuration (within configuration itself)
foreach(SUBSYSTEM ${PRE-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	add_dependencies(pre-configuration ${SUBSYSTEM})
endforeach()

# default dependencies on post-configuration (within configuration itself)
foreach(SUBSYSTEM ${POST-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	add_dependencies(${SUBSYSTEM} pre-configuration)
endforeach()

# default dependencies on configuraiton (with outside world)
# 	at top-level CMakeLists.txt, we defined configuration dependencies
#		for example: add_dependencies(configuration tcl_stubb_build)
#   this default dependencis make sure all the subsystem inherit configuration dependencies
foreach(SUBSYSTEM ${SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	add_dependencies(configuration ${SUBSYSTEM})
endforeach()

foreach(SUBSYSTEM ${NO-COMMON-PRE-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	target_link_libraries(${SUBSYSTEM} INTERFACE cfgcommon)
endforeach()

foreach(SUBSYSTEM ${POST-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	target_link_libraries(${SUBSYSTEM} INTERFACE cfgcommon)
endforeach()

# Custom dependencies
# 	This is custom dependencies within configuration subsystem
# Example BitAssembler use CFGObject library 
target_link_libraries(bitassembler INTERFACE cfgobject)