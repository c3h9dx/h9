/*
 * H9 project
 *
 * Created by SQ8KFH on 2023-09-08.
 *
 * Copyright (C) 2023-2024 Kamil Palkowski. All rights reserved.
 */

#include "h9d_configurator.h"

#include <cxxopts/cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <spdlog/cfg/helpers.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <stdarg.h>
#include <unistd.h>

#include "loop_driver.h"
#include "udp_driver.h"
#include "slcan_driver.h"
#include "socketcan_driver.h"
#include "git_version.h"
#include "libconfuse_helper.h"

namespace {
std::string last_confuse_error_message = "";
}

static void cfg_err_func(cfg_t* cfg, const char* fmt, va_list args) {
    char msgbuf[1024];
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
    last_confuse_error_message = msgbuf;
    SPDLOG_TRACE(msgbuf);
}

H9dConfigurator::H9dConfigurator():
    log_file(""),
    override_pid_file(""),
    config_file(default_config),
    cfg(nullptr),
    log_file_sink(nullptr),
    debug(false),
    verbose(0),
    override_daemonize(false),
    foreground(false) {
}

H9dConfigurator::~H9dConfigurator() {
    if (cfg)
        cfg_free(cfg);
}

void H9dConfigurator::parse_command_line_arg(int argc, char** argv) {
    cxxopts::Options options("h9d", "H9 daemon.");
    // clang-format off
    options.add_options("other")
#ifdef H9_DEBUG
            ("D,debug", "Enable debugging")
#endif
            ("h,help", "Show help")
            ("v,verbose", "Be more verbose")
            ("V,version", "Show version")
            ;
    options.add_options("daemon")
            ("c,config", "Config file", cxxopts::value<std::string>()->default_value(default_config))
            ("d,daemonize", "Run in the background")
            ("f,foreground", "Prevent go in the background")
            ("l,logfile", "Log file", cxxopts::value<std::string>())
            ("L,logger_level", "Logger level modification (e.g. 'frame=debug,bus=trace')", cxxopts::value<std::string>())
            ("p,pidfile", "PID file", cxxopts::value<std::string>())
            ;
    // clang-format on
    try {
        cxxopts::ParseResult result = options.parse(argc, argv);

        debug = result.count("debug");

        if (result.count("help")) {
            std::cerr << options.help({"", "daemon", "other"}) << std::endl;
            exit(EXIT_SUCCESS);
        }

        if (result.count("version")) {
            std::cerr << "h9d version " << version_string() << " by crowx." << std::endl;
            std::cerr << "Copyright (C) 2017-2023 Kamil Palkowski. All rights reserved." << std::endl;
            exit(EXIT_SUCCESS);
        }

        verbose = result.count("verbose");

        if (result.count("config")) {
            config_file = std::string(result["config"].as<std::string>());
        }

        if (result.count("daemonize")) {
            override_daemonize = true;
        }

        if (result.count("foreground")) {
            foreground = true;
        }

        if (result.count("logfile")) {
            log_file = std::string(result["logfile"].as<std::string>());
        }

        if (result.count("logger_level")) {
            logger_level_setting_string = result["logger_level"].as<std::string>();
        }

        if (result.count("pidfile")) {
            override_pid_file = std::string(result["pidfile"].as<std::string>());
        }
    }
    catch (cxxopts::exceptions::parsing e) {
        std::cerr << e.what() << ". Try '-h' for more information." << std::endl;
        exit(EXIT_FAILURE);
    }
}

void H9dConfigurator::logger_initial_setup() {
    if (log_file.empty()) {
        auto h9 = spdlog::stdout_color_mt(default_logger_name);
    }
    else {
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            log_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file);

            std::vector<spdlog::sink_ptr> sinks = {console_sink, log_file_sink};
            auto h9 = std::make_shared<spdlog::logger>(default_logger_name, std::begin(sinks), std::end(sinks));

            h9->flush_on(spdlog::level::info);

            spdlog::register_logger(h9);
        }
        catch (spdlog::spdlog_ex e) {
            std::cerr << "Can not open log file: '" << log_file << "'." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    auto h9 = spdlog::get(default_logger_name);

    if (debug)
        h9->set_pattern(log_debug_pattern);
    else
        h9->set_pattern(log_pattern);

    spdlog::set_default_logger(h9);

    int tmp_level = h9->level() - verbose;
    if (tmp_level < spdlog::level::trace)
        h9->set_level(spdlog::level::trace);
    else
        h9->set_level(static_cast<spdlog::level::level_enum>(tmp_level));
}

void H9dConfigurator::logger_setup() {
    auto bus = spdlog::stdout_color_mt(bus_logger_name);
    auto frame_console = spdlog::stdout_color_mt(frames_logger_name);
    auto tcpserver_console = spdlog::stdout_color_mt(tcp_logger_name);
    auto devices_console = spdlog::stdout_color_mt(nodes_logger_name);
    auto vendpoint_console = spdlog::stdout_color_mt(vendpoint_logger_name);

    if (log_file_sink) {
        bus->sinks().push_back(log_file_sink);
        frame_console->sinks().push_back(log_file_sink);
        tcpserver_console->sinks().push_back(log_file_sink);
        devices_console->sinks().push_back(log_file_sink);
        vendpoint_console->sinks().push_back(log_file_sink);
    }

    spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l) {
        if (debug)
            l->set_pattern(log_debug_pattern);
        else
            l->set_pattern(log_pattern);

        int tmp_level = l->level() - verbose;
        if (tmp_level < spdlog::level::trace)
            l->set_level(spdlog::level::trace);
        else
            l->set_level(static_cast<spdlog::level::level_enum>(tmp_level));
    });

    spdlog::cfg::helpers::load_levels(logger_level_setting_string);

    cfg_t* cfg_log = cfg_getsec(cfg, "log");
    std::shared_ptr<spdlog::sinks::basic_file_sink<spdlog::details::null_mutex>> frames_file_sink = nullptr;

    char* frames_logfile = nullptr;
    if (cfg_log) {
        frames_logfile = cfg_getstr(cfg_log, "frames_logfile");
        if (frames_logfile) {
            try {
                frames_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(frames_logfile);
            }
            catch (spdlog::spdlog_ex e) {
                frames_file_sink = nullptr;
                SPDLOG_ERROR("Frames logger can not open file: {} - frames logging to the file disabled.", frames_logfile);
            }
        }
    }

    auto frames_recv_file = std::make_shared<spdlog::logger>(frames_recv_to_file_logger_name, frames_file_sink);
    auto frames_sent_file = std::make_shared<spdlog::logger>(frames_sent_to_file_logger_name, frames_file_sink);

    frames_recv_file->flush_on(spdlog::level::info);
    frames_sent_file->flush_on(spdlog::level::info);

    class frame_direction_flag: public spdlog::custom_flag_formatter {
      public:
        void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
            std::string direction = "?";
            if (msg.logger_name == frames_recv_to_file_logger_name) {
                direction = "in";
            }
            else if (msg.logger_name == frames_sent_to_file_logger_name) {
                direction = "out";
            }
            dest.append(direction.data(), direction.data() + direction.size());
        }

        std::unique_ptr<custom_flag_formatter> clone() const override {
            return spdlog::details::make_unique<frame_direction_flag>();
        }
    };

    if (!frames_file_sink) {
        frames_recv_file->set_level(spdlog::level::off);
        frames_sent_file->set_level(spdlog::level::off);
    }
    else {
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<frame_direction_flag>('*').set_pattern(
            R"({"date": "%Y-%m-%dT%T.%e", "direction": "%*", %v})");
        frames_file_sink->set_formatter(std::move(formatter));

        if (cfg_getbool(cfg_log, "disable_recv_frames"))
            frames_recv_file->set_level(spdlog::level::off);
        else
            SPDLOG_INFO("Enabled logging recv frames to file: {}", frames_logfile);

        if (cfg_getbool(cfg_log, "disable_sent_frames"))
            frames_sent_file->set_level(spdlog::level::off);
        else
            SPDLOG_INFO("Enabled logging sent frames to file: {}", frames_logfile);
    }

    spdlog::register_logger(frames_sent_file);
    spdlog::register_logger(frames_recv_file);
}

void H9dConfigurator::load_configuration() {
    SPDLOG_INFO("Loading configuration from file '{}'...", config_file);

    cfg_opt_t cfg_process_sec[] = {
        CFG_BOOL("daemonize", cfg_false, CFGF_NONE),
        CFG_STR("pidfile", nullptr, CFGF_NONE),
        CFG_INT("setuid", 0, CFGF_NONE),
        CFG_INT("setgid", 0, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_server_sec[] = {
        CFG_INT("port", H9D_DEFAULT_PORT, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_log_sec[] = {
        // TODO: dodac log level
        CFG_STR("frames_logfile", nullptr, CFGF_NONE),
        CFG_BOOL("disable_sent_frames", cfg_false, CFGF_NONE),
        CFG_BOOL("disable_recv_frames", cfg_false, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_endpoint_sec[] = {
        CFG_STR("driver", nullptr, CFGF_NONE),
        CFG_STR("tty", nullptr, CFGF_NONE),
        CFG_STR("interface", nullptr, CFGF_NONE),
        CFG_STR("local_port", nullptr, CFGF_NONE),
        CFG_STR("remote_port", nullptr, CFGF_NONE),
        CFG_STR("remote_addr", nullptr, CFGF_NONE),
        CFG_END()};

    cfg_opt_t cfg_bus_opts[] = {
        CFG_INT("source_id", default_source_id, CFGF_NONE),
        CFG_BOOL("forwarding", cfg_true, CFGF_NONE),
        CFG_INT("response_timeout_duration", default_response_timeout_duration, CFGF_NONE),
        CFG_STR("nodes_description_filename", nullptr, CFGF_NONE),
        CFG_STR("devs_configuration_filename", nullptr, CFGF_NONE),
        CFG_SEC("endpoint", cfg_endpoint_sec, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()};

    cfg_opt_t cfg_virtual_endpoint_sec[] = {
        CFG_STR("python_path", ".", CFGF_NONE),
        CFG_STR("python_node_module", nullptr, CFGF_NONE | CFGF_NODEFAULT),
        CFG_INT("node_id", 0, CFGF_NONE | CFGF_NODEFAULT),
        CFG_INT("node_type", 0, CFGF_NONE | CFGF_NODEFAULT),
        CFG_END()};

    cfg_opt_t cfg_opts[] = {
        CFG_SEC("process", cfg_process_sec, CFGF_NONE),
        CFG_SEC("server", cfg_server_sec, CFGF_NONE),
        CFG_SEC("bus", cfg_bus_opts, CFGF_NONE),
        CFG_SEC("virtual_endpoint", cfg_virtual_endpoint_sec, CFGF_NONE | CFGF_NODEFAULT),
        CFG_SEC("log", cfg_log_sec, CFGF_NONE),
        CFG_END()};

    cfg = cfg_init(cfg_opts, CFGF_NONE);
    cfg_set_error_function(cfg, cfg_err_func);
    cfg_set_validate_func(cfg, "bus|source_id", confuse_helpers::validate_node_id);
    cfg_set_validate_func(cfg, "virtual_endpoint|node_id", confuse_helpers::validate_node_id);
    cfg_set_validate_func(cfg, "virtual_endpoint|node_type", confuse_helpers::validate_node_type);

    int ret = cfg_parse(cfg, config_file.c_str());

    if (ret == CFG_FILE_ERROR) {
        SPDLOG_CRITICAL("Unable to load a file: '{}'.", config_file);
        cfg_free(cfg);
        cfg = nullptr;
        exit(EXIT_FAILURE);
    }
    else if (ret == CFG_PARSE_ERROR) {
        SPDLOG_CRITICAL("Config file parse error: {}.", last_confuse_error_message);
        last_confuse_error_message = "";
        cfg_free(cfg);
        cfg = nullptr;
        exit(EXIT_FAILURE);
    }
}

void H9dConfigurator::daemonize() {
    if (foreground)
        return;

    cfg_t* cfg_process = cfg_getsec(cfg, "process");
    if (override_daemonize || (cfg_process && cfg_getbool(cfg_process, "daemonize"))) {

#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
#endif
        if (daemon(1, 0) != 0) {
#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
            SPDLOG_CRITICAL("Unable to daemonize: %s.", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else {
            SPDLOG_INFO("Going to background - be like the daemon.");
        }
    }
}

void H9dConfigurator::save_pid() {
    std::string pid_file = "";
    if (override_pid_file.empty()) {
        cfg_t* cfg_process = cfg_getsec(cfg, "process");
        if (cfg_process) {
            char* ret = cfg_getstr(cfg_process, "pidfile");
            pid_file = ret ? ret : "";
        }
    }
    else {
        pid_file = override_pid_file;
    }
    if (!pid_file.empty()) {
        auto pid = getpid();
        std::ofstream _pid_file;
        _pid_file.open(pid_file, std::ofstream::out | std::ofstream::trunc);
        if (_pid_file.is_open()) {
            _pid_file << pid;
            _pid_file.close();
            SPDLOG_INFO("Saved PID ({}) in file: '{}'.", pid, pid_file);
        }
        else {
            SPDLOG_ERROR("Unable to save a PID in file: '{}'.", pid_file);
        }
    }
}

void H9dConfigurator::drop_privileges() {
    cfg_t* cfg_process = cfg_getsec(cfg, "process");
    if (cfg_process) {
        int uid = cfg_getint(cfg_process, "setuid");
        int gid = cfg_getint(cfg_process, "setgid");

        if (gid > 0) {
            if (setgid(gid) != 0) {
                SPDLOG_CRITICAL("Unable to drop group privileges to GID: {} - {}.", gid, strerror(errno));
                exit(EXIT_FAILURE);
            }
            else {
                SPDLOG_INFO("Dropped GID to: {}", gid);
            }
        }
        if (uid > 0) {
            if (getuid() != 0) {
                SPDLOG_ERROR("Unable to drop user privileges to GID: {} - you UID ({}) is not 0.", uid, getuid());
            }
            else if (setuid(uid) != 0) {
                SPDLOG_CRITICAL("Unable to drop user privileges to UID: {} - {}.", uid, strerror(errno));
                exit(EXIT_FAILURE);
            }
            else {
                SPDLOG_INFO("Dropped UID to: {}.", uid);
            }
        }
    }
}

void H9dConfigurator::configure_bus(Bus* bus, VirtualEndpoint* vendpoint) {
    cfg_t* cfg_bus= cfg_getsec(cfg, "bus");
    bus->bus_default_source_id(cfg_getint(cfg_bus, "source_id"));

    bool tmp = cfg_getbool(cfg_bus, "forwarding") == cfg_true;
    bus->forwarding(tmp);

    int n = cfg_size(cfg_bus, "endpoint");
    for (int i = 0; i < n; i++) {
        cfg_t* endpoint_section = cfg_getnsec(cfg_bus, "endpoint", i);
        // if (bus_driver_section) {
        std::string endpoint_name = cfg_title(endpoint_section);
        if (cfg_getstr(endpoint_section, "driver")) {
            std::string driver = cfg_getstr(endpoint_section, "driver");
            if (driver == "loop") {
                bus->add_driver(new LoopDriver(endpoint_name));
            }
            else if (driver == "udp") {
                std::string local_port;
                std::string remote_port;
                std::string remote_addr;

                if (cfg_getstr(endpoint_section, "local_port")) {
                    local_port = cfg_getstr(endpoint_section, "local_port");
                }
                else {
                    SPDLOG_CRITICAL("Missing option 'local_port' for {}.", cfg_title(endpoint_section));
                    exit(EXIT_FAILURE);
                }
                if (cfg_getstr(endpoint_section, "remote_port")) {
                    remote_port = cfg_getstr(endpoint_section, "remote_port");
                }
                else {
                    SPDLOG_CRITICAL("Missing option 'remote_port' for {}.", cfg_title(endpoint_section));
                    exit(EXIT_FAILURE);
                }
                if (cfg_getstr(endpoint_section, "remote_addr")) {
                    remote_addr = cfg_getstr(endpoint_section, "remote_addr");
                }
                else {
                    SPDLOG_CRITICAL("Missing option 'remote_addr' for {}.", cfg_title(endpoint_section));
                    exit(EXIT_FAILURE);
                }
                bus->add_driver(new UDPDriver(endpoint_name, local_port, remote_addr, remote_port));
            }
            else if (driver == "virtual" && vendpoint) {
                if (vendpoint->is_configured()) {
                    bus->add_driver(vendpoint->get_driver(endpoint_name));
                }
                else {
                    SPDLOG_ERROR("Unable add '{}' endpoint, the virtual endpoint is not configured.", endpoint_name);
                }
            }
            else if (driver == "SLCAN") {
                if (cfg_getstr(endpoint_section, "tty")) {
                    std::string tty = cfg_getstr(endpoint_section, "tty");
                    bus->add_driver(new SlcanDriver(endpoint_name, tty));
                }
                else {
                    SPDLOG_CRITICAL("Missing option 'tty' for {}.", cfg_title(endpoint_section));
                    exit(EXIT_FAILURE);
                }
            }
#ifdef H9_SOCKETCAN_DRIVER
                else if (driver == "SocketCAN") {
                if (cfg_getstr(endpoint_section, "interface")) {
                    std::string interface = cfg_getstr(endpoint_section, "interface");
                    bus->add_driver(new SocketCANDriver(endpoint_name, interface));
                    // SocketCANDriver("can0", "can0");
                }
                else {
                    SPDLOG_ERROR("Missing option 'interface' for {}.", cfg_title(endpoint_section));
                }
            }
#endif
            else {
                SPDLOG_CRITICAL("Unsupported driver: '{}' for '{}' bus.", driver, endpoint_name);
                exit(EXIT_FAILURE);
            }
        }
        else {
            SPDLOG_CRITICAL("Missing option 'driver' for {}.", cfg_title(endpoint_section));
            exit(EXIT_FAILURE);
        }
    }

    if (bus->endpoint_count() == 0) {
        SPDLOG_CRITICAL("The bus must contain at least one endpoint.");
        exit(EXIT_FAILURE);
    }
}

void H9dConfigurator::configure_virtual_endpoint(VirtualEndpoint* vendpoint) {
    cfg_t* cfg_vend= cfg_getsec(cfg, "virtual_endpoint");
    if (cfg_vend) {
        std::string python_path = cfg_getstr(cfg_vend, "python_path");
        vendpoint->set_python_path(python_path);

//        if (cfg_getstr(cfg_vend, "python_path")) {
//
//        }
//        else {
//            SPDLOG_CRITICAL("Missing option 'python_path' in virtual_endpoint section.");
//            exit(EXIT_FAILURE);
//        }

        std::string python_module;
        if (cfg_getstr(cfg_vend, "python_node_module")) {
            python_module = cfg_getstr(cfg_vend, "python_node_module");
        }
        else {
            SPDLOG_CRITICAL("Missing option 'python_node_module' in virtual_endpoint section.");
            exit(EXIT_FAILURE);
        }

        if (cfg_getint(cfg_vend, "node_id")) {
            int id = cfg_getint(cfg_vend, "node_id");
            if (cfg_getint(cfg_vend, "node_type")) {
                int node_type = cfg_getint(cfg_vend, "node_type");
                vendpoint->add_node(id, node_type, python_module);
            }
            else {
                SPDLOG_CRITICAL("Missing option 'node_type' in virtual_endpoint section or it is equal 0.");
                exit(EXIT_FAILURE);
            }
        }
        else {
            SPDLOG_CRITICAL("Missing option 'node_id' in virtual_endpoint section or it is equal 0.");
            exit(EXIT_FAILURE);
        }
    }
}

void H9dConfigurator::configure_devices_mgr(NodeMgr* devices_mgr) {
    cfg_t* cfg_bus= cfg_getsec(cfg, "bus");
    devices_mgr->response_timeout_duration(cfg_getint(cfg_bus, "response_timeout_duration"));
    if (cfg_getstr(cfg_bus, "nodes_description_filename")) {
        devices_mgr->load_nodes_description(cfg_getstr(cfg_bus, "nodes_description_filename"));
    }
    if (cfg_getstr(cfg_bus, "devs_configuration_filename")) {
        devices_mgr->load_devs_configuration(cfg_getstr(cfg_bus, "devs_configuration_filename"));
    }
}

void H9dConfigurator::configure_tcpserver(TCPServer* server) {
    cfg_t* cfg_server = cfg_getsec(cfg, "server");
    if (cfg_server) {
        std::uint16_t port = cfg_getint(cfg_server, "port");
        server->set_server_port(port);
    }
}

std::string H9dConfigurator::version_string() {
    std::string ret = H9_VERSION;
#ifdef GITVERSION_COMMIT_SHA
    ret += "-" + std::string(GITVERSION_COMMIT_SHA);
#ifdef GITVERSION_DIRTY
    ret += "-dirty";
    constexpr char workdir[] = "dirty";
#endif
#endif
    return std::move(ret);
}

std::string H9dConfigurator::version() {
    return std::move(std::string(H9_VERSION));
}

std::string H9dConfigurator::version_commit_sha() {
#ifdef GITVERSION_COMMIT_SHA
    return std::move(std::string(GITVERSION_COMMIT_SHA));
#endif
    return std::move(std::string(""));
}

bool H9dConfigurator::version_dirty() {
#ifdef GITVERSION_DIRTY
    return true;
#endif
    return false;
}
