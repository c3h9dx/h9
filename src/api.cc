/*
 * H9 project
 *
 * Created by crowx on 2023-09-11.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#include "api.h"

#include "config.h"

#include <spdlog/spdlog.h>

#include "bus.h"
#include "dev_node_exception.h"
#include "dev_status_observer.h"
#include "h9d_configurator.h"
#include "node_dev_mgr.h"
#include "tcpclientthread.h"
#include "tcpserver.h"

nlohmann::json API::get_version(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r({{"version", H9dConfigurator::version()}});

    if (!H9dConfigurator::version_commit_sha().empty())
        r["commit_sha"] = H9dConfigurator::version_commit_sha();
    if (H9dConfigurator::version_dirty())
        r["dirty"] = H9dConfigurator::version_dirty();
    return std::move(r);
}

nlohmann::json API::get_methods_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json methods;
    for (const auto& [method, v] : api_methods) {
        methods.push_back(method);
    }
    nlohmann::json r = {
        {"methods_list", std::move(methods)},
    };

    return std::move(r);
}

nlohmann::json API::subscribe(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    try {
        std::string event_name = params.param_map.at("event").get<std::string>();

        if (event_name == "frame") {
            client_thread->set_frame_observer(new ClientFrameObs(client_thread, bus, H9FrameComparator()));
        }
        else if (event_name == "dev_status") {
            client_thread->set_dev_status_observer(new DevStatusObserver(client_thread, node_dev_mgr));
        }

        nlohmann::json r = {{"return", "ok"}};

        SPDLOG_DEBUG("subscribe: {}", event_name);
        return std::move(r);
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
}

nlohmann::json API::get_tcp_clients(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r = nlohmann::json::array();
    for(auto& dsc : tcp_server->get_clients_list()) {
        char ct[std::size("yyyy-mm-ddThh:mm:ssZ")];
        std::strftime(std::data(ct), std::size(ct), "%FT%TZ", std::gmtime(&dsc.connection_time));

        r.push_back({
                { "id", dsc.idstring },
                { "entity", dsc.entity },
                { "connection_time", ct },
                { "authenticated", dsc.authenticated },
                { "remote_address", dsc.remote_address },
                { "remote_port", dsc.remote_port },
                { "frame_subscription", dsc.frame_subscription },
                { "dev_subscription", dsc.dev_subscription },
        });

    }
    return std::move(r);
}

nlohmann::json API::send_frame(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    try {
        bool raw = false;
        if (params.param_map.count("raw") && params.param_map.at("raw").get<bool>()) {
            raw = true;
        }
        ExtH9Frame frame = params.param_map.at("frame").get<ExtH9Frame>(); // nie zlapiemy wyjatkow! gdzies po drodze jest noexcept

        if (raw && frame.invalid_member() & ~ExtH9Frame::VALID_ORIGIN) {
            SPDLOG_ERROR("Incorrect raw frame during invoke 'send_frame' by {}", __FUNCTION__, client_thread->get_client_idstring());
            SPDLOG_DEBUG("Raw 'send_frame' dump frame params: {}.", __FUNCTION__, params.param_map.at("frame").dump());
            throw jsonrpcpp::InvalidParamsException("Incorrect raw frame during invoke 'send_frame'", id);
        }
        else if (frame.invalid_member() & ~(ExtH9Frame::VALID_ORIGIN | ExtH9Frame::VALID_SEQNUM | ExtH9Frame::VALID_SOURCE_ID)) {
            SPDLOG_ERROR("Incorrect frame invoke 'send_frame' by {}", __FUNCTION__, client_thread->get_client_idstring());
            SPDLOG_DEBUG("'send_frame' dump frame params: {}.", __FUNCTION__, params.param_map.at("frame").dump());
            throw jsonrpcpp::InvalidParamsException("Incorrect frame during invoke 'send_frame'", id);
        }

        frame.origin(client_thread->get_client_idstring());
        int ret = bus->send_frame(std::move(frame), raw);

        nlohmann::json r = {{"seqnum", ret}};

        SPDLOG_DEBUG("Call send_frame - raw: {}, result seqnum: {}", raw, ret);
        return std::move(r);
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
}

nlohmann::json API::get_stats(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    // std::uint32_t d = 256;
    // node_mgr->get_node(16).reset(client_thread->get_client_idstring());
    // node_mgr->get_node(16).set_reg(client_thread->get_client_idstring(), 12, d);
    return std::move(MetricsCollector::metrics_to_json());
}

nlohmann::json API::authenticate(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    try {
        std::string entity = params.param_map.at("entity").get<std::string>();
        client_thread->authenticate(entity);
        nlohmann::json r = {{"authentication", true}};
        return std::move(r);
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
}

nlohmann::json API::get_nodes_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r = nlohmann::json::array();
    for (auto& d : node_dev_mgr->get_nodes_list()) {
        r.push_back({{"id", d.id}, {"type", d.type}, {"name", d.name}});
    }
    return std::move(r);
}

nlohmann::json API::get_node_info(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    NodeDevMgr::NodeInfo device_info;
    if (node_dev_mgr->get_node_info(node_id, device_info) < 0) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error("Node " + std::to_string(node_id) + "does not exist.", NODE_IS_NOT_EXIST), id);
    }

    char ct[std::size("yyyy-mm-ddThh:mm:ssZ")];
    char lst[std::size("yyyy-mm-ddThh:mm:ssZ")];
    std::strftime(std::data(ct), std::size(ct), "%FT%TZ", std::gmtime(&device_info.created_time));
    std::strftime(std::data(lst), std::size(lst), "%FT%TZ", std::gmtime(&device_info.last_seen_time));

    nlohmann::json r = nlohmann::json({
        {"id", device_info.id},
        {"type", device_info.type},
        {"version_major", device_info.version_major},
        {"version_minor", device_info.version_minor},
        {"version_patch", device_info.version_patch},
        {"name", device_info.name},
        {"created_time", ct},
        {"last_seen_time", lst},
        {"description", device_info.description},
    });
    return std::move(r);
}

nlohmann::json API::discover_nodes(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    node_dev_mgr->discover();
    return true;
}

nlohmann::json API::node_reset(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    try {
        node_dev_mgr->node_reset(node_id);
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return true;
}

nlohmann::json API::get_registers_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    if (node_dev_mgr->is_node_exist(node_id)) {
        nlohmann::json r = nlohmann::json::array();

        for (auto& reg : node_dev_mgr->get_registers_list(node_id)) {
            r.push_back({{"number", reg.number},
                         {"name", reg.name},
                         {"type", reg.type},
                         {"size", reg.size},
                         {"readable", reg.readable},
                         {"writable", reg.writable},
                         {"bits_names", reg.bits_names},
                         {"description", reg.description}});
        }
        return std::move(r);
    }
    throw jsonrpcpp::RequestException(jsonrpcpp::Error("Node " + std::to_string(node_id) + "does not exist.", NODE_IS_NOT_EXIST), id);
}

nlohmann::json API::get_register_value(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    std::uint8_t reg = 0;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
        reg = params.param_map.at("reg").get<std::uint8_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        auto res = node_dev_mgr->get_register(node_id, reg);
        if (std::holds_alternative<std::int64_t>(res)) {
            r = std::get<std::int64_t>(res);
        }
        else if (std::holds_alternative<std::string>(res)) {
            r = std::get<std::string>(res);
        }
        else if (std::holds_alternative<std::vector<std::uint8_t>>(res)) {
            r = std::get<std::vector<std::uint8_t>>(res);
        }
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::set_register_value(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    std::uint8_t reg = 0;
    Node::regvalue_t val;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
        reg = params.param_map.at("reg").get<std::uint8_t>();
        if (params.param_map.at("value").type() == nlohmann::json::value_t::string) {
            val = params.param_map.at("value").get<std::string>();
        }
        else if (params.param_map.at("value").type() == nlohmann::json::value_t::number_integer || params.param_map.at("value").type() == nlohmann::json::value_t::number_unsigned) {
            val = params.param_map.at("value").get<std::int64_t>();
        }
        else if (params.param_map.at("value").type() == nlohmann::json::value_t::array) {
            val = params.param_map.at("value").get<std::vector<std::uint8_t>>();
        }
        else {
            SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - Unsupported JSON ({}) as a value", __FUNCTION__, client_thread->get_client_idstring(), params.param_map.at("value").type_name());
            SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
            throw jsonrpcpp::InvalidParamsException("Unsupported JSON type as a value", id);
        }
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        auto res = node_dev_mgr->set_register(node_id, reg, val);
        if (std::holds_alternative<std::int64_t>(res)) {
            r = std::get<std::int64_t>(res);
        }
        else if (std::holds_alternative<std::string>(res)) {
            r = std::get<std::string>(res);
        }
        else if (std::holds_alternative<std::vector<std::uint8_t>>(res)) {
            r = std::get<std::vector<std::uint8_t>>(res);
        }
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::set_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    std::uint8_t reg = 0;
    std::uint8_t bit_num;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
        reg = params.param_map.at("reg").get<std::uint8_t>();
        bit_num = params.param_map.at("bit_num").get<std::uint8_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        auto res = node_dev_mgr->set_register_bit(node_id, reg, bit_num);
        if (std::holds_alternative<std::int64_t>(res)) {
            r = std::get<std::int64_t>(res);
        }
        else if (std::holds_alternative<std::string>(res)) {
            r = std::get<std::string>(res);
        }
        else if (std::holds_alternative<std::vector<std::uint8_t>>(res)) {
            r = std::get<std::vector<std::uint8_t>>(res);
        }
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::clear_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    std::uint8_t reg = 0;
    std::uint8_t bit_num;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
        reg = params.param_map.at("reg").get<std::uint8_t>();
        bit_num = params.param_map.at("bit_num").get<std::uint8_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        auto res = node_dev_mgr->clear_register_bit(node_id, reg, bit_num);
        if (std::holds_alternative<std::int64_t>(res)) {
            r = std::get<std::int64_t>(res);
        }
        else if (std::holds_alternative<std::string>(res)) {
            r = std::get<std::string>(res);
        }
        else if (std::holds_alternative<std::vector<std::uint8_t>>(res)) {
            r = std::get<std::vector<std::uint8_t>>(res);
        }
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::toggle_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::uint16_t node_id = 0xffff;
    std::uint8_t reg = 0;
    std::uint8_t bit_num;
    try {
        node_id = params.param_map.at("node_id").get<std::uint16_t>();
        reg = params.param_map.at("reg").get<std::uint8_t>();
        bit_num = params.param_map.at("bit_num").get<std::uint8_t>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        auto res = node_dev_mgr->toggle_register_bit(node_id, reg, bit_num);
        if (std::holds_alternative<std::int64_t>(res)) {
            r = std::get<std::int64_t>(res);
        }
        else if (std::holds_alternative<std::string>(res)) {
            r = std::get<std::string>(res);
        }
        else if (std::holds_alternative<std::vector<std::uint8_t>>(res)) {
            r = std::get<std::vector<std::uint8_t>>(res);
        }
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::get_devs_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r = nlohmann::json::array();
    for (auto& d : node_dev_mgr->get_devs_list()) {
        r.push_back({{"name", d.name}, {"type", d.type}});
    }
    return std::move(r);
}

nlohmann::json API::get_dev_description(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::string dev_name;

    try {
        dev_name = params.param_map.at("dev_name").get<std::string>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        r = node_dev_mgr->get_dev_description(dev_name, client_thread, id, params);
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::get_dev_status(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::string dev_name;

    try {
        dev_name = params.param_map.at("dev_name").get<std::string>();
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        r = node_dev_mgr->get_dev_state(dev_name, client_thread, id, params);
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

nlohmann::json API::dev_method_call(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::string dev_name;

    try {
        dev_name = params.param_map.at("dev_name").get<std::string>();
        params.param_map.at("method").get<std::string>(); // exist check
    }
    catch (std::out_of_range& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }
    catch (nlohmann::detail::type_error& e) {
        SPDLOG_ERROR("Incorrect parameters during invoke '{}' by {} - {}", __FUNCTION__, client_thread->get_client_idstring(), e.what());
        SPDLOG_DEBUG("Dump '{}' calling params: {}.", __FUNCTION__, params.to_json().dump());
        throw jsonrpcpp::InvalidParamsException(e.what(), id);
    }

    nlohmann::json r;

    try {
        r = node_dev_mgr->call_dev_method(dev_name, client_thread, id, params);
    }
    catch (DevNodeException& e) {
        throw jsonrpcpp::RequestException(jsonrpcpp::Error(e.what(), e.code()), id);
    }

    return std::move(r);
}

API::API(Bus* bus, NodeDevMgr* dev_mgr):
    bus(bus),
    node_dev_mgr(dev_mgr) {
    api_methods["get_version"] = &API::get_version;
    api_methods["get_methods_list"] = &API::get_methods_list;
    api_methods["subscribe"] = &API::subscribe;
    api_methods["get_tcp_clients"] = &API::get_tcp_clients;
    api_methods["send_frame"] = &API::send_frame;
    api_methods["get_stats"] = &API::get_stats;
    api_methods["authenticate"] = &API::authenticate;
    api_methods["get_nodes_list"] = &API::get_nodes_list;
    api_methods["get_node_info"] = &API::get_node_info;
    api_methods["discover_nodes"] = &API::discover_nodes;
    api_methods["node_reset"] = &API::node_reset;
    api_methods["get_registers_list"] = &API::get_registers_list;
    api_methods["get_register_value"] = &API::get_register_value;
    api_methods["set_register_value"] = &API::set_register_value;
    api_methods["set_register_bit"] = &API::set_register_bit;
    api_methods["clear_register_bit"] = &API::clear_register_bit;
    api_methods["toggle_register_bit"] = &API::toggle_register_bit;
    api_methods["get_devs_list"] = &API::get_devs_list;
    api_methods["get_dev_description"] = &API::get_dev_description;
    api_methods["get_dev_status"] = &API::get_dev_status;
    api_methods["dev_method_call"] = &API::dev_method_call;
}

void API::set_tcp_server(TCPServer* tcp_server) {
    this->tcp_server = tcp_server;
}

jsonrpcpp::Response API::call(TCPClientThread* client_thread, const jsonrpcpp::request_ptr& request) {
    if (api_methods.count(request->method()) && (client_thread->authenticated() || request->method() == "authenticate")) {
        nlohmann::json r = (this->*api_methods[request->method()])(client_thread, request->id(), request->params());

        jsonrpcpp::Response res(request->id(), r);
        return std::move(res);
    }
    else {
        throw jsonrpcpp::MethodNotFoundException(request->id());
    }
}
