cmake_minimum_required(VERSION 3.15)

set(cfg_path ${CMAKE_CURRENT_SOURCE_DIR})
project(configurationRS)

add_custom_target(pre-configurationRS
	COMMAND
		echo "This is top level build for Pre-ConfigurationRS subsystem. It does not really build any target but it defines all subsystem or subdirectory that need to be built in configuration first"
	)

add_custom_target(configurationRS
	COMMAND
		echo "This is top level build for ConfigurationRS subsystem. It does not really build any target but it defines all subsystem or subdirectory and their dependencies"
	)

add_custom_target(post-configurationRS
	COMMAND
		echo "This is top level build for Post-ConfigurationRS subsystem. It does not really build any target but it defines all subsystem that need to run after regular build for example unit test"
	)

# Define the subsystem here
list(APPEND LEVEL1_SUBSYSTEMS CFGCommonRS)
list(APPEND LEVEL2_SUBSYSTEMS CFGCrypto CFGObject)
list(APPEND LEVEL3_SUBSYSTEMS BitAssembler BitGenerator Ocla)
list(APPEND EXE_SUBSYSTEMS BitAssembler BitGenerator Ocla)

# Only change this if you know what it does
list(APPEND PRE-SUBSYSTEMS ${LEVEL1_SUBSYSTEMS} ${LEVEL2_SUBSYSTEMS})
list(APPEND POST-SUBSYSTEMS ${LEVEL3_SUBSYSTEMS})
list(APPEND SUBSYSTEMS ${PRE-SUBSYSTEMS} ${POST-SUBSYSTEMS})

# set parent scope directory definition before subdirectory is added so that it can be inherited
#	in each subdirectory avoid using using ".." relative path -- it is very hard to manage if we have multiple layer
set(CFG_PROJECT_ROOT_DIR ${PROJECT_SOURCE_DIR}/../..)
set(CFG_FOEDAG_PROJECT_ROOT_DIR ${CFG_PROJECT_ROOT_DIR}/FOEDAG/src/Configuration)
set(CFG_FOEDAGRS_PROJECT_ROOT_DIR ${CFG_PROJECT_ROOT_DIR}/src/ConfigurationRS)
set(CFG_BUILD_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../..)
message("ConfigurationRS source dirctory is ${PROJECT_SOURCE_DIR}")
message("ConfigurationRS binary dirctory is ${CMAKE_CURRENT_BINARY_DIR}")
message("ConfigurationRS set project root directory as ${CFG_PROJECT_ROOT_DIR}")
message("ConfigurationRS set build root directory as ${CFG_BUILD_ROOT_DIR}")

# Determine the library type for configuration
set(CFG_LIB_TYPE STATIC)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CFG_BUILD_ROOT_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CFG_BUILD_ROOT_DIR}/bin)

# subdirectory
foreach(SUBSYSTEM ${SUBSYSTEMS})
	add_subdirectory(${SUBSYSTEM})
endforeach()

# include configuration to all subsystem
foreach(SUBSYSTEM ${SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	target_include_directories(${SUBSYSTEM} PUBLIC
				${CFG_PROJECT_ROOT_DIR}
        ${CFG_PROJECT_ROOT_DIR}/FOEDAG/src
        ${CFG_PROJECT_ROOT_DIR}/src
        ${CFG_PROJECT_ROOT_DIR}/src/ConfigurationRS
        ${CFG_BUILD_ROOT_DIR}/include
        ${CFG_BUILD_ROOT_DIR}/FOEDAG/src
        ${CFG_BUILD_ROOT_DIR}/FOEDAG/src/Configuration
        ${CFG_BUILD_ROOT_DIR}/src
        ${CFG_BUILD_ROOT_DIR}/src/ConfigurationRS
        ${CFG_BUILD_ROOT_DIR}/FOEDAG/include
    )
endforeach()

if(MSVC)
	foreach(SUBSYSTEM ${SUBSYSTEMS})
		list(FIND EXE_SUBSYSTEMS ${SUBSYSTEM} subsystem_index)	
		string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
		set_property(TARGET ${SUBSYSTEM} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		set_property(TARGET ${SUBSYSTEM} PROPERTY COMPILER_FLAGS /DSTATIC_BUILD)
		set_target_properties(${SUBSYSTEM} PROPERTIES
			COMPILE_OPTIONS "$<$<CONFIG:Debug>:/MTd>$<$<CONFIG:Release>:/MT>"
		)
		# Every subsystem will have test
		set_property(TARGET ${SUBSYSTEM}_test 
			PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
		set_property(TARGET ${SUBSYSTEM}_test 
			PROPERTY COMPILER_FLAGS /DSTATIC_BUILD
		)
		set_target_properties(${SUBSYSTEM}_test PROPERTIES
			COMPILE_OPTIONS "$<$<CONFIG:Debug>:/MTd>$<$<CONFIG:Release>:/MT>"
		)
		if (${subsystem_index} GREATER -1)
			# Currently only ${POST-SUBSYSTEMS} has raptor_xxxx
			set_property(TARGET raptor_${SUBSYSTEM} 
				PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
			)
			set_property(TARGET raptor_${SUBSYSTEM}
				PROPERTY COMPILER_FLAGS /DSTATIC_BUILD
			)
			set_target_properties(raptor_${SUBSYSTEM} PROPERTIES
				COMPILE_OPTIONS "$<$<CONFIG:Debug>:/MTd>$<$<CONFIG:Release>:/MT>"
			)
		endif()
	endforeach()
endif()

# default dependencies on pre-configurationRS (within configuration itself)
foreach(SUBSYSTEM ${PRE-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	add_dependencies(pre-configurationRS ${SUBSYSTEM})
endforeach()

# default dependencies on post-configuration (within configuration itself)
foreach(SUBSYSTEM ${POST-SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	add_dependencies(${SUBSYSTEM} pre-configurationRS)
endforeach()

# default dependencies on configuraiton (with outside world)
# 	at top-level CMakeLists.txt, we can define configurationRS dependencies
#   this default dependencis make sure all the subsystem inherit configurationRS dependencies
set(RUN_CONFIG_UNIT_TEST 0)
if (DEFINED ENV{UNIT_TEST})
  if ($ENV{UNIT_TEST} STREQUAL "1")
    set(RUN_CONFIG_UNIT_TEST 1)
  endif()
endif()
if (${RUN_CONFIG_UNIT_TEST} EQUAL 1)
  foreach(SUBSYSTEM ${SUBSYSTEMS})
    string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
    add_custom_target(post-${SUBSYSTEM} ALL
      COMMAND echo "Run unit test for subsystem ${SUBSYSTEM}"
      COMMAND ${CFG_BUILD_ROOT_DIR}/bin/${SUBSYSTEM}_test
    )
    add_dependencies(post-${SUBSYSTEM} ${SUBSYSTEM})
    add_dependencies(post-configurationRS post-${SUBSYSTEM})
  endforeach()
else()
  foreach(SUBSYSTEM ${SUBSYSTEMS})
    string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
    add_dependencies(post-configurationRS ${SUBSYSTEM})
  endforeach()
endif()
add_dependencies(configurationRS post-configurationRS) 

# default dependencies on configuraiton (within configurationRS)
foreach(SUBSYSTEM ${SUBSYSTEMS})
	string(TOLOWER ${SUBSYSTEM} SUBSYSTEM)
	if (NOT SUBSYSTEM STREQUAL "cfgcommonrs")
		target_link_libraries(${SUBSYSTEM} INTERFACE cfgcommonrs)
	endif()
endforeach()

# Custom dependencies
# 	This is custom dependencies within configuration subsystem
# Example BitAssembler use CFGObject library
target_link_libraries(cfgcommonrs INTERFACE cfgcommon)
target_link_libraries(bitgenerator INTERFACE cfgcrypto)
target_link_libraries(bitassembler INTERFACE cfgobject)
target_link_libraries(bitgenerator INTERFACE cfgobject)
target_link_libraries(bitassembler INTERFACE bitgenerator)

add_dependencies(bitassembler nlohmann_json)
add_dependencies(bitgenerator nlohmann_json)
add_dependencies(cfgcrypto openssl_build)