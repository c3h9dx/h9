/*
 * Created by crowx on 26/10/2023.
 *
 */

#include "command_executor.h"

#include <fmt/chrono.h>
#include <iostream>
#include <spdlog/spdlog.h>

#include "fmt_helpers.h"

CommandExecutor::CommandExecutor(H9Connector* h9d_conn):
    h9d(h9d_conn) {
}

void CommandExecutor::get_and_parse_response(const jsonrpcpp::Id& request_id, const std::string& method) {
    jsonrpcpp::entity_ptr raw_msg;
    try {
        raw_msg = h9d->recv();
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.what());
        exit(EXIT_FAILURE);
    }

    SPDLOG_TRACE("Received msg: {}.", raw_msg->to_json().dump());

    if (raw_msg->is_response()) {
        jsonrpcpp::response_ptr msg = std::dynamic_pointer_cast<jsonrpcpp::Response>(raw_msg);

        // if (msg->is_error()) { //it is now work
        if (msg->error().operator bool()) {
            auto err = msg->error();

            if (msg->id().int_id() != request_id.int_id() && err.code() != -32700) {
                SPDLOG_ERROR("Reguest, response id missmach: {} != {}.", request_id.int_id(), msg->id().int_id());
                SPDLOG_DEBUG("Dump response: {}", msg->to_json().dump());
            }

            printf("Error (%d): %s\n", err.code(), err.message().c_str());
        }
        else {
            if (msg->id().int_id() != request_id.int_id()) {
                SPDLOG_ERROR("Reguest, response id missmach: {} != {}.", request_id.int_id(), msg->id().int_id());
                SPDLOG_DEBUG("Dump response: {}", msg->to_json().dump());
            }

            auto result = msg->result();
            print_response(method, msg->result());
        }
    }
    else {
        SPDLOG_ERROR("Type {} message received, expected 'response' or 'error'.", raw_msg->type_str());
        SPDLOG_DEBUG("Dump response: {}", raw_msg->to_json().dump());
    }
}

void CommandExecutor::print_response(const std::string& method, const nlohmann::json& result) {
    if (method == "get_node_info") {
        tm ct_utc;
        strptime(result["created_time"].get<std::string>().c_str(), "%FT%TZ", &ct_utc);
        tm lst_utc;
        strptime(result["last_seen_time"].get<std::string>().c_str(), "%FT%TZ", &lst_utc);

        time_t ct_utc_ts = timegm(&ct_utc);
        time_t lst_utc_ts = timegm(&lst_utc);

        std::tm ct_tm;
        localtime_r(&ct_utc_ts, &ct_tm);
        std::tm lst_tm;
        localtime_r(&lst_utc_ts, &lst_tm);

        auto now = std::time(nullptr);

        fmt::print("Name:          {}\n", result["name"].get<std::string>());
        fmt::print("ID:            {}\n", result["id"].get<std::uint16_t>());
        fmt::print("Type:          {}\n", result["type"].get<std::uint16_t>());
        fmt::print("Version:       {}.{}{}\n", result["version_major"].get<std::uint16_t>(), result["version_minor"].get<std::uint16_t>(), result["hardware_revision"].get<char>());
        fmt::print("Reset reason:  {}\n", result["reset_reason"].get<std::uint8_t >());
        fmt::print("Created time:  {:%F %T} ({} ago)\n", ct_tm, format_uptime(now - ct_utc_ts));
        fmt::print("Last seen:     {:%F %T} ({} ago)\n", lst_tm, format_uptime(now - lst_utc_ts));
        fmt::print("Description:   {}\n", result["description"].get<std::string>());
    }
    else if (method == "get_tcp_clients") {
        for (auto& client : result) {
            tm ct_utc;
            strptime(client["connection_time"].get<std::string>().c_str(), "%FT%TZ", &ct_utc);
            time_t ct_utc_ts = timegm(&ct_utc);
            std::tm ct_tm;
            localtime_r(&ct_utc_ts, &ct_tm);

            auto now = std::time(nullptr);

            fmt::print("{}\n", client["id"].get<std::string>());
            fmt::print("    entity:              {}\n", client["entity"].get<std::string>());
            fmt::print("    remote address:      {}\n", client["remote_address"].get<std::string>());
            fmt::print("    remote port:         {}\n", client["remote_port"].get<std::string>());
            fmt::print("    connection time:     {:%F %T} (for {})\n", ct_tm, format_uptime(now - ct_utc_ts));
            fmt::print("    authenticated:       {}\n", client["authenticated"].get<bool>());
            fmt::print("    frame subscription:  {}\n", client["frame_subscription"].get<bool>());
            fmt::print("    dev subscription:    {}\n", client["dev_subscription"].get<bool>());
            if (client != result.back())
                fmt::print("\n");
        }
    }
    else if (method == "get_nodes_list") {
        for (auto& node : result) {
            fmt::print("{}:\n", node["id"].get<unsigned int>());
            fmt::print("    type:       {}\n", node["type"].get<unsigned int>());
            fmt::print("    type name:  {}\n", node["name"].get<std::string>());
            if (node != result.back())
                fmt::print("\n");
        }
    }
    else if (method == "get_devs_list") {
        for (auto& dev : result) {
            fmt::print("{}:\n", dev["name"].get<std::string>());
            fmt::print("    type:       {}\n", dev["type"].get<std::string>());
            if (dev != result.back())
                fmt::print("\n");
        }
    }
    else {
        std::cout << result << std::endl;
    }
}

void CommandExecutor::execute(const jsonrpcpp::Request& r) {
    jsonrpcpp::Id id(h9d->get_next_id());
    jsonrpcpp::Request request = {id, r.method(), r.params()};

    SPDLOG_DEBUG("Sending msg: {}.", request.to_json().dump().c_str());
    h9d->send(std::make_shared<jsonrpcpp::Request>(request));

    get_and_parse_response(id, r.method());
}
