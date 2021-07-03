PROJECT(chain-socket)
cmake_minimum_required(VERSION 3.2)
list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_SOURCE_DIR}/cmake/macros")
include(AutoCollect)
set(CMAKE_CXX_STANDARD 17)
# cd build
# cmake .. -G "MinGW Makefiles" -DCMAKE_SH="CMAKE_SH-NOTFOUND"
PickIncludeDirectories(
        ${CMAKE_CURRENT_SOURCE_DIR}/code
        INCLUDEDIRS
        # Exclude
        ${CMAKE_CURRENT_SOURCE_DIR}/build)

PickSourceFiles(
        ${CMAKE_CURRENT_SOURCE_DIR}/code
        CODEFILES
        # Exclude
        ${CMAKE_CURRENT_SOURCE_DIR}/build)

MakeFilter(
        ${CMAKE_CURRENT_SOURCE_DIR}/code)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    #    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g")
ENDIF ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
#set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -O -static")
ADD_EXECUTABLE(${CMAKE_PROJECT_NAME}
        ${CODEFILES} code/ChainSocket/xmalloc.cpp code/ChainSocket/xmalloc.h code/ChainSocket/Ip.cpp code/ChainSocket/Ip.h)
target_include_directories(${CMAKE_PROJECT_NAME}
        PUBLIC ${INCLUDEDIRS})

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${CMAKE_PROJECT_NAME} ws2_32)
ELSE ()
    target_link_libraries(${CMAKE_PROJECT_NAME} pthread)
ENDIF ()



