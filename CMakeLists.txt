cmake_minimum_required(VERSION 2.8)

project(task C ASM)

set(HEADERS 
	include/task.h
)

set(SOURCES 
	src/task.c
)

if(WIN32)
	if(CMAKE_GENERATOR MATCHES "64")
		set(SOURCES ${SOURCES}
			src/win64.asm)
		set_source_files_properties(src/win64.asm PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
	endif(CMAKE_GENERATOR MATCHES "64")
else(WIN32)
	set(SOURCES ${SOURCES}
		src/asm.s)
	set_source_files_properties(src/asm.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
endif(WIN32)

macro(GroupSources curdir)
	file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
			GroupSources(${curdir}/${child})
		else()
			string(REPLACE "/" "\\" groupname ${curdir})
			source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
		endif()
	endforeach()
endmacro()

GroupSources(include)
GroupSources(src)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_library(task SHARED ${HEADERS} ${SOURCES})
set_target_properties(task PROPERTIES COMPILE_DEFINITIONS "TASK_BUILD_SHARED=1")
if(UNIX AND NOT APPLE)
    target_link_libraries(task rt)
endif(UNIX AND NOT APPLE)

install(TARGETS task DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_WRITE GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE)
install(FILES ${HEADERS} DESTINATION include/task)

project(fibonacci C)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(fibonacci samples/fibonacci.c)
target_link_libraries(fibonacci task)

project(ping-pong C)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(ping-pong samples/ping-pong.c)
target_link_libraries(ping-pong task)

project(permutations C)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(permutations samples/permutations.c)
target_link_libraries(permutations task)

project(communication C)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(communication samples/communication.c)
target_link_libraries(communication task)