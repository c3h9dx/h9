/*
 * H9 project
 *
 * Created by SQ8KFH on 2019-04-09.
 *
 * Copyright (C) 2019-2023 Kamil Palkowski. All rights reserved.
 */

#include "config.h"

#include <cstdlib>
#include <cstring>
#include <jsonrpcpp/jsonrpcpp.hpp>
#include <spdlog/cfg/helpers.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <unistd.h>

#include "api.h"
#include "bus.h"
#include "h9d_configurator.h"
#include "metrics_collector.h"
#include "node_mgr.h"
#include "tcpserver.h"
#include "virtual_endpoint.h"

int main(int argc, char** argv) {
    std::set_terminate([]() {
        std::exception_ptr ex = std::current_exception();
        try {
            if (ex) std::rethrow_exception(ex);
        }
        catch (const std::bad_exception& e) {
            SPDLOG_CRITICAL("Bad exception: {}", e.what());
        }
        catch (const std::exception& e) {
            SPDLOG_CRITICAL("Unhandled exception from: {}", e.what());
        }
        std::abort();
    });

    H9dConfigurator configurator;

    configurator.parse_command_line_arg(argc, argv);
    configurator.logger_initial_setup();

    SPDLOG_WARN("Starting h9d... Version: {}.", configurator.version_string());

    configurator.load_configuration();
    configurator.logger_setup();
    configurator.daemonize();
    configurator.save_pid();
    configurator.drop_privileges();

    VirtualEndpoint virtual_endpoint;
    configurator.configure_virtual_endpoint(&virtual_endpoint);

    Bus bus;
    configurator.configure_bus(&bus, &virtual_endpoint);
    bus.activate();

    virtual_endpoint.activate();

    NodeMgr devices_mgr(&bus);
    configurator.configure_devices_mgr(&devices_mgr);

    devices_mgr.discover();

    API api(&bus, &devices_mgr);

    TCPServer server(&api);
    configurator.configure_tcpserver(&server);

    api.set_tcp_server(&server);

    server.run();

    return EXIT_FAILURE;
}
