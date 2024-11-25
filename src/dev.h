/*
 * Created by crowx on 29/10/2023.
 *
 */

#pragma once

#include <functional>
#include <jsonrpcpp/jsonrpcpp.hpp>
#include <map>

#include "node.h"
#include "node_mgr.h"

class DevStatusObserver;

class Dev {
  private:
    const std::vector<std::uint16_t> dependent_on_nodes;
    std::map<std::string, std::function<nlohmann::json(const std::map<std::string, nlohmann::json>& param_map)>> method_map;

    struct rusage usage_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> total_start;

    double user_cpu_time;
    double system_cpu_time;
    double total_time;

  protected:
    NodeMgr* node_mgr;
    Dev(std::string type, std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes);
    void emit_dev_state(const nlohmann::json& dev_status);

    template<typename DevClass>
    void add_method(const std::string& method_name, nlohmann::json (DevClass::* m)(const std::map<std::string, nlohmann::json>&)) {
        method_map[method_name] = std::bind(m, dynamic_cast<DevClass*>(this), std::placeholders::_1);
    }

  public:
    const std::string type;
    const std::string name;

    virtual ~Dev();

    void cpu_usage_begin();
    void cpu_usage_end();

    const std::vector<std::uint16_t>& get_nodes_id();

    nlohmann::json get_dev_description(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json call_dev_method(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);

    virtual void periodic_task() {};
    virtual void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame);

    virtual bool is_init() = 0;
    virtual void init() {};

    virtual void on_dev_info(std::uint16_t node_id, std::uint16_t type, std::uint16_t version_major, std::uint16_t version_minor, char hardware_revision) {};
    virtual void on_dev_register_value(std::uint16_t node_id, uint8_t reg, Node::regvalue_t value) {};
    virtual void on_dev_error(std::uint16_t node_id, uint8_t error_number) {};
    virtual void on_dev_bulk(std::uint16_t node_id, H9frame::Type msg_type, uint8_t dlc, const uint8_t* data) {};

    virtual nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map);
};
