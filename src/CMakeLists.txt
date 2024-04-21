if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    message("-! Using epoll")
    SET(H9D_EVENT epoll.cc epoll.h)

    message("-! Enable SocketCAN driver")
    SET(H9_SOCKETCAN_DRIVER socketcan_driver.cc socketcan_driver.h)
else()
    message("-! Using kqueue")
    SET(H9D_EVENT kqueue.cc kqueue.h)

    message("-! Disable SocketCAN driver")
    SET(H9_SOCKETCAN_DRIVER "")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-psabi)
    SET(ATOMIC_LINK atomic)
endif()

if(H9_DEBUG)
    message("-! Debug enable")
    set(H9_DEBUG TRUE)
    set(NDEBUG "")
else()
    message("-! Debug disable")
    set(H9_DEBUG "")
    set(NDEBUG TRUE)
endif()

configure_file(config_gen.h.in config_gen.h)

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

set(H9D_SRC h9d.cc config.h
        h9d_configurator.cc h9d_configurator.h
        h9frame.cc h9frame.h
        ext_h9frame.cc ext_h9frame.h
        busframe.cc busframe.h
        bus_driver.cc bus_driver.h
        loop_driver.cc loop_driver.h
        slcan_driver.cc slcan_driver.h
        ${H9_SOCKETCAN_DRIVER}
        ${H9D_EVENT}
        bus.cc bus.h
        framesubject.cc frameobserver.h
        frameobserver.cc frameobserver.h
        h9framecomparator.cc h9framecomparator.h
        tcpserver.cc tcpserver.h
        tcpclientthread.cc tcpclientthread.h
        h9socket.cc h9socket.h
        h9msgsocket.cc h9msgsocket.h
        api.cc api.h
        client_frame_obs.cc client_frame_obs.h
        metrics_collector.cc metrics_collector.h
        raw_node.cc raw_node.h
        send_frame_result.h
        node_dev_mgr.cc node_dev_mgr.h
        node.cc node.h
        node_desc_loader.cc node_desc_loader.h
        virtual_driver.cc virtual_driver.h
        virtual_endpoint.cc virtual_endpoint.h
        virtual_py_node.cc virtual_py_node.h
        py_h9.cc py_h9.h
        dev_node_exception.cc dev_node_exception.h
        dev.cc dev.h
        antenna_switch_dev.cc antenna_switch_dev.h
        dev_status_observer.cc dev_status_observer.h
        fmt_helpers.cc fmt_helpers.h
        dev_loader.cc dev_loader.h
        libconfuse_helper.cc libconfuse_helper.h udp_driver.cc udp_driver.h)

set(CLIENT_COMMON_SRC config.h
        h9frame.cc h9frame.h
        ext_h9frame.cc ext_h9frame.h
        h9socket.cc h9socket.h
        h9msgsocket.cc h9msgsocket.h
        h9connector.cc h9connector.h
        h9_configurator.cc h9_configurator.h
        fmt_helpers.cc fmt_helpers.h
        libconfuse_helper.cc libconfuse_helper.h)

set(H9SNIFFER_SRC h9sniffer.cc config.h)

set(H9SEND_SRC h9send.cc config.h)

set(H9STATS_SRC h9stats.cc config.h)

set(H9FWUPLOAD_SRC h9fwupload.cc config.h config_gen.h.in firmware_uploader.cc firmware_uploader.h)

set(H9CLI_SRC h9cli.cc config.h config_gen.h.in cli_parsing_driver.cc cli_parsing_driver.h cli_cache.cc cli_cache.h command_executor.cc command_executor.h)

add_custom_command(OUTPUT git_version.h
                  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/generate_git_version_header.py > ${CMAKE_CURRENT_BINARY_DIR}/git_version.h~
                  COMMAND /bin/sh -c 'diff ${CMAKE_CURRENT_BINARY_DIR}/git_version.h~ ${CMAKE_CURRENT_BINARY_DIR}/git_version.h 2> /dev/null > /dev/null || { cp ${CMAKE_CURRENT_BINARY_DIR}/git_version.h~ ${CMAKE_CURRENT_BINARY_DIR}/git_version.h\; }' DEPENDS ${H9D_SRC})


add_executable(h9d ${H9D_SRC} git_version.h)
target_include_directories(h9d PUBLIC ${CONFUSE_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})
target_compile_options(h9d PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9d PUBLIC ${CONFUSE_LIBRARIES} PRIVATE ${ATOMIC_LINK} fmt::fmt nlohmann_json::nlohmann_json spdlog::spdlog Python3::Python)
install(TARGETS h9d RUNTIME DESTINATION bin)

add_library(CLIENT_COMMON OBJECT ${CLIENT_COMMON_SRC} git_version.h)
target_include_directories(CLIENT_COMMON PUBLIC ${CONFUSE_INCLUDE_DIRS})
target_link_libraries(CLIENT_COMMON fmt::fmt)

add_executable(h9sniffer ${H9SNIFFER_SRC} $<TARGET_OBJECTS:CLIENT_COMMON>)
target_include_directories(h9sniffer PUBLIC ${CONFUSE_INCLUDE_DIRS})
target_compile_options(h9sniffer PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9sniffer PUBLIC ${CONFUSE_LIBRARIES} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)
install(TARGETS h9sniffer RUNTIME DESTINATION bin)

add_executable(h9send ${H9SEND_SRC} $<TARGET_OBJECTS:CLIENT_COMMON>)
target_include_directories(h9send PUBLIC ${CONFUSE_INCLUDE_DIRS})
target_compile_options(h9send PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9send PUBLIC ${CONFUSE_LIBRARIES} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)
install(TARGETS h9send RUNTIME DESTINATION bin)

add_executable(h9stats ${H9STATS_SRC} $<TARGET_OBJECTS:CLIENT_COMMON>)
target_include_directories(h9stats PUBLIC ${CONFUSE_INCLUDE_DIRS})
target_compile_options(h9stats PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9stats PUBLIC ${CONFUSE_LIBRARIES} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)
install(TARGETS h9stats RUNTIME DESTINATION bin)

add_executable(h9fwupload ${H9FWUPLOAD_SRC} $<TARGET_OBJECTS:CLIENT_COMMON>)
target_include_directories(h9fwupload PUBLIC ${CONFUSE_INCLUDE_DIRS})
target_compile_options(h9fwupload PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9fwupload PUBLIC ${CONFUSE_LIBRARIES} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)
install(TARGETS h9fwupload RUNTIME DESTINATION bin)

BISON_TARGET(cli_parser cli_parser.y ${CMAKE_CURRENT_BINARY_DIR}/cli_parser.cc DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/cli_parser.h)
FLEX_TARGET(cli_scaner cli_lexer.l ${CMAKE_CURRENT_BINARY_DIR}/cli_lexer.cc)
ADD_FLEX_BISON_DEPENDENCY(cli_scaner cli_parser)
add_executable(h9cli ${H9CLI_SRC} ${BISON_cli_parser_OUTPUTS} ${FLEX_cli_scaner_OUTPUTS} $<TARGET_OBJECTS:CLIENT_COMMON>)
target_include_directories(h9cli PUBLIC ${CMAKE_SOURCE_DIR}/src ${CONFUSE_INCLUDE_DIRS})
target_compile_options(h9cli PUBLIC ${CONFUSE_CFLAGS_OTHER})
target_link_libraries(h9cli PUBLIC ${READLINE_LIBRARIES} ${CONFUSE_LIBRARIES} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)
install(TARGETS h9cli RUNTIME DESTINATION bin)
