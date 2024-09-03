/*
 * Created by crowx on 29/10/2023.
 *
 */

#include "dev.h"

#include <utility>

#include "dev_status_observer.h"

Dev::Dev(std::string type, std::string name, NodeDevMgr* node_mgr, std::vector<std::uint16_t> nodes):
    type(std::move(type)),
    name(std::move(name)),
    node_mgr(node_mgr),
    dependent_on_nodes(std::move(nodes)) {

    for (auto node : dependent_on_nodes) {
        node_mgr->attach_node_state_observer(node, this);
    }
}

Dev::~Dev() {
    SPDLOG_TRACE("~Dev() {}", fmt::ptr(this));
    for (auto node : dependent_on_nodes) {
        node_mgr->detach_node_state_observer(node, this);
    }
}

void Dev::emit_dev_state(const nlohmann::json& dev_status) {
    node_mgr->emit_dev_state(name, dev_status);
}

const std::vector<std::uint16_t>& Dev::get_nodes_id() {
    return dependent_on_nodes;
}

nlohmann::json Dev::get_dev_description(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::vector<std::string> methods;

    methods.reserve(method_map.size());
    for (const auto& [k, v] : method_map) {
        methods.push_back(k);
    }

    return {{"dev_name", name},
            {"dev_type", type},
            {"dev_related_nodes", dependent_on_nodes},
            {"dev_methods", methods}};
}

nlohmann::json Dev::call_dev_method(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    if (params.param_map.count("method") == 0) {
        throw jsonrpcpp::InvalidParamsException("Missing 'method' parameter.", id);
    }

    std::string method = params.param_map.at("method").get<std::string>();

    if (method_map.count(method)) {
        try {
            return method_map[method](params.param_map);
        }
        catch (std::exception& e) {
            throw jsonrpcpp::InvalidParamsException("Method '" + method + "' execution error: " + e.what() + ".", id);
        }
    }
    else {
        throw jsonrpcpp::InvalidParamsException("Dev object does not provide '" + method + "' method.", id);
    }
}

void Dev::init() {

}

nlohmann::json Dev::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {};
}
