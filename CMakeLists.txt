cmake_minimum_required(VERSION 3.24)

project(h9 VERSION 0.3.1)

set(CMAKE_CXX_STANDARD 23)

option(H9_DEBUG "Enable debug")

set(H9_VERSION ${PROJECT_VERSION})

find_package(Python3 3.10 REQUIRED Interpreter Development)
find_package(PkgConfig REQUIRED)

find_package(fmt 9.0 REQUIRED)
find_package(spdlog 1.10 REQUIRED)
find_package(nlohmann_json 3.11.2 REQUIRED)

find_package(FLEX 2.6 REQUIRED)
find_package(BISON 3.8 REQUIRED)

pkg_check_modules(CONFUSE libconfuse REQUIRED)
pkg_check_modules(READLINE readline>=8.0)

message("-! Compiler ${CMAKE_CXX_COMPILER_ID}")
message("-! Python version ${Python3_VERSION}")

if(NOT READLINE_FOUND)
    message(STATUS "Checking for module 'readline>=7.0'")
    include(cmake/FindReadline.cmake)
    if(Readline_VERSION VERSION_GREATER_EQUAL "7.0") 
	    message(STATUS "  Found readline ${Readline_VERSION}")
    else()
	    message(FATAL_ERROR "  No package 'readline' found")
    endif()
    set(READLINE_LIBRARIES ${Readline_LIBRARIES})
    set(READLINE_INCLUDE_DIRS ${Readline_INCLUDE_DIRS})
endif()

if (DEFINED ENV{H9CAN_PATH})
    set(H9CAN_PATH $ENV{H9CAN_PATH})
    message("H9CAN_PATH is: " ${H9CAN_PATH})
else ()
    set(H9CAN_PATH ${PROJECT_SOURCE_DIR}/h9can)
    message("H9CAN_PATH not set, using local: " ${H9CAN_PATH})
endif ()

include(${H9CAN_PATH}/cmake/unix.cmake)

include_directories(${CMAKE_SOURCE_DIR}/thirdparty/ ${CMAKE_CURRENT_BINARY_DIR}/src/)

link_directories(/usr/local/lib/ /opt/local/lib/)

add_subdirectory(src)

install(FILES conf/h9d.conf DESTINATION /etc/h9)
install(FILES conf/h9.conf DESTINATION /etc/h9)
install(FILES conf/nodes.conf DESTINATION /etc/h9)
install(FILES conf/dev.conf DESTINATION /etc/h9)
install(FILES systemd/h9d.service DESTINATION /etc/systemd/system)
install(DIRECTORY virtual_node DESTINATION lib/h9)
install(DIRECTORY DESTINATION /var/log/h9)

#enable_testing()
#
#add_test(
#        NAME cpp_test
#        COMMAND false
#)