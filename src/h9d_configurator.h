/*
 * H9 project
 *
 * Created by SQ8KFH on 2023-09-08.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include <confuse.h>
#include <string>

#include "bus.h"
#include "node_mgr.h"
#include "tcpserver.h"
#include "virtual_endpoint.h"

class H9dConfigurator {
  public:
    constexpr static char default_logger_name[] = "h9d";
    constexpr static char bus_logger_name[] = "bus";
    constexpr static char frames_logger_name[] = "frame";
    constexpr static char frames_recv_to_file_logger_name[] = "frames_recv_to_file";
    constexpr static char frames_sent_to_file_logger_name[] = "frames_sent_to_file";
    constexpr static char tcp_logger_name[] = "tcp";
    constexpr static char nodes_logger_name[] = "node";
    constexpr static char vendpoint_logger_name[] = "vend";

    constexpr static int default_source_id = 509;
    constexpr static int default_response_timeout_duration = 5;
  private:
    constexpr static char log_debug_pattern[] = "%^[%L %T.%e] [%n:%t] [%s:%#]%$ %v";
    constexpr static char log_pattern[] = "%^[%L %T.%e] [%n:%t]%$ %v";

    constexpr static const char* default_config = H9D_CONFIG_FILE;

    bool debug;
    int verbose;
    bool override_daemonize;
    bool foreground;
    std::string log_file;
    std::string logger_level_setting_string;
    std::string config_file;
    std::string override_pid_file;

    cfg_t* cfg;
    spdlog::sink_ptr log_file_sink;

  public:
    H9dConfigurator();
    ~H9dConfigurator();
    void parse_command_line_arg(int argc, char** argv);
    void logger_initial_setup();
    void logger_setup();
    void load_configuration();

    void daemonize();
    void save_pid();
    void drop_privileges();

    void configure_bus(Bus* bus, VirtualEndpoint* vendpoint);
    void configure_virtual_endpoint(VirtualEndpoint* vendpoint);
    void configure_devices_mgr(NodeMgr* devices_mgr);
    void configure_tcpserver(TCPServer* server);

    static std::string version_string();
    static std::string version();
    static std::string version_commit_sha();
    static bool version_dirty();
};
